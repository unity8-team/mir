/*
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "MirInputDispatcher"
#define ATRACE_TAG ATRACE_TAG_INPUT

//#define LOG_NDEBUG 0

// Log detailed debug messages about each inbound event notification to the dispatcher.
#define DEBUG_INBOUND_EVENT_DETAILS 0

// Log detailed debug messages about each outbound event processed by the dispatcher.
#define DEBUG_OUTBOUND_EVENT_DETAILS 0

// Log debug messages about the dispatch cycle.
#define DEBUG_DISPATCH_CYCLE 0

// Log debug messages about registrations.
#define DEBUG_REGISTRATION 0

// Log debug messages about input focus tracking.
#define DEBUG_FOCUS 0

// Log debug messages about the app switch latency optimization.
#define DEBUG_APP_SWITCH 0

#include "mirinputdispatcher.h"

#include <mir/input/input_report.h>

#include <cutils/log.h>
#include <android/keycodes.h>

#include <stddef.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <time.h>

#define INDENT "  "
#define INDENT2 "    "
#define INDENT3 "      "
#define INDENT4 "        "

#include <QDebug>

namespace mi = mir::input;

namespace android {

// Default input dispatching timeout if there is no focused application or paused window
// from which to determine an appropriate dispatching timeout.
const nsecs_t DEFAULT_INPUT_DISPATCHING_TIMEOUT = 5000 * 1000000LL; // 5 sec

// Amount of time to allow for all pending events to be processed when an app switch
// key is on the way.  This is used to preempt input dispatch and drop input events
// when an application takes too long to respond and the user has pressed an app switch key.
const nsecs_t APP_SWITCH_TIMEOUT = 500 * 1000000LL; // 0.5sec

// Amount of time to allow for an event to be dispatched (measured since its eventTime)
// before considering it stale and dropping it.
const nsecs_t STALE_EVENT_TIMEOUT = 10000 * 1000000LL; // 10sec

// Amount of time to allow touch events to be streamed out to a connection before requiring
// that the first event be finished.  This value extends the ANR timeout by the specified
// amount.  For example, if streaming is allowed to get ahead by one second relative to the
// queue of waiting unfinished events, then ANRs will similarly be delayed by one second.
const nsecs_t STREAM_AHEAD_EVENT_TIMEOUT = 500 * 1000000LL; // 0.5sec

// Log a warning when an event takes longer than this to process, even if an ANR does not occur.
const nsecs_t SLOW_EVENT_PROCESSING_WARNING_TIMEOUT = 2000 * 1000000LL; // 2sec

MirInputDispatcherInterface *MirInputDispatcherInterface::gInstance = nullptr;

static inline nsecs_t now() {
    return systemTime(SYSTEM_TIME_MONOTONIC);
}

static inline const char* toString(bool value) {
    return value ? "true" : "false";
}

static inline int32_t getMotionEventActionPointerIndex(int32_t action) {
    return (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK)
            >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
}

static bool isValidKeyAction(int32_t action) {
    switch (action) {
    case AKEY_EVENT_ACTION_DOWN:
    case AKEY_EVENT_ACTION_UP:
        return true;
    default:
        return false;
    }
}

static bool validateKeyEvent(int32_t action) {
    if (! isValidKeyAction(action)) {
        ALOGE("Key event has invalid action code 0x%x", action);
        return false;
    }
    return true;
}

static bool isValidMotionAction(int32_t action, size_t pointerCount) {
    switch (action & AMOTION_EVENT_ACTION_MASK) {
    case AMOTION_EVENT_ACTION_DOWN:
    case AMOTION_EVENT_ACTION_UP:
    case AMOTION_EVENT_ACTION_CANCEL:
    case AMOTION_EVENT_ACTION_MOVE:
    case AMOTION_EVENT_ACTION_OUTSIDE:
    case AMOTION_EVENT_ACTION_HOVER_ENTER:
    case AMOTION_EVENT_ACTION_HOVER_MOVE:
    case AMOTION_EVENT_ACTION_HOVER_EXIT:
    case AMOTION_EVENT_ACTION_SCROLL:
        return true;
    case AMOTION_EVENT_ACTION_POINTER_DOWN:
    case AMOTION_EVENT_ACTION_POINTER_UP: {
        int32_t index = getMotionEventActionPointerIndex(action);
        return index >= 0 && size_t(index) < pointerCount;
    }
    default:
        return false;
    }
}

static bool validateMotionEvent(int32_t action, size_t pointerCount,
        const PointerProperties* pointerProperties) {
    if (! isValidMotionAction(action, pointerCount)) {
        ALOGE("Motion event has invalid action code 0x%x", action);
        return false;
    }
    if (pointerCount < 1 || pointerCount > MAX_POINTERS) {
        ALOGE("Motion event has invalid pointer count %d; value must be between 1 and %d.",
                pointerCount, MAX_POINTERS);
        return false;
    }
    IntSet pointerIds;
    for (size_t i = 0; i < pointerCount; i++) {
        int32_t id = pointerProperties[i].id;
        if (id < 0 || id > MAX_POINTER_ID) {
            ALOGE("Motion event has invalid pointer id %d; value must be between 0 and %d",
                    id, MAX_POINTER_ID);
            return false;
        }
        if (pointerIds.contains(id)) {
            ALOGE("Motion event has duplicate pointer id %d", id);
            return false;
        }
        pointerIds.insert(id);
    }
    return true;
}

// --- MirInputDispatcher ---

MirInputDispatcher::MirInputDispatcher(std::shared_ptr<mi::InputReport> const& input_report) :
        input_report(input_report),
        mPendingEvent(NULL), mAppSwitchSawKeyDown(false), mAppSwitchDueTime(LONG_LONG_MAX),
        mNextUnblockedEvent(NULL),
        mDispatchEnabled(false), mDispatchFrozen(false),
        mInputTargetWaitCause(INPUT_TARGET_WAIT_CAUSE_NONE) {
    mLooper = new Looper(false);
}

MirInputDispatcher::~MirInputDispatcher() {
    { // acquire lock
        AutoMutex _l(mLock);

        releasePendingEventLocked();
        drainInboundQueueLocked();
    }

    while (mConnectionsByFd.size() != 0) {
        unregisterInputChannel(mConnectionsByFd.valueAt(0)->inputChannel);
    }
}

void MirInputDispatcher::dispatchOnce() {
    nsecs_t nextWakeupTime = LONG_LONG_MAX;
    { // acquire lock
        AutoMutex _l(mLock);
        broadcast(mDispatcherIsAliveCondition);

        dispatchOnceInnerLocked(&nextWakeupTime);

        if (runCommandsLockedInterruptible()) {
            nextWakeupTime = LONG_LONG_MIN;  // force next poll to wake up immediately
        }
    } // release lock

    // Wait for callback or timeout or wake.  (make sure we round up, not down)
    nsecs_t currentTime = now();
    int timeoutMillis = toMillisecondTimeoutDelay(currentTime, nextWakeupTime);
    mLooper->pollOnce(timeoutMillis);
}

void MirInputDispatcher::dispatchOnceInnerLocked(nsecs_t* nextWakeupTime) {
    nsecs_t currentTime = now();

    // If dispatching is frozen, do not process timeouts or try to deliver any new events.
    if (mDispatchFrozen) {
#if DEBUG_FOCUS
        printf("Dispatch frozen.  Waiting some more.\n");
#endif
        return;
    }

    // Optimize latency of app switches.
    // Essentially we start a short timeout when an app switch key (HOME / ENDCALL) has
    // been pressed.  When it expires, we preempt dispatch and drop all other pending events.
    bool isAppSwitchDue = mAppSwitchDueTime <= currentTime;
    if (mAppSwitchDueTime < *nextWakeupTime) {
        *nextWakeupTime = mAppSwitchDueTime;
    }

    // Ready to start a new event.
    // If we don't already have a pending event, go grab one.
    if (! mPendingEvent) {
        if (mInboundQueue.isEmpty()) {
            if (isAppSwitchDue) {
                // The inbound queue is empty so the app switch key we were waiting
                // for will never arrive.  Stop waiting for it.
                resetPendingAppSwitchLocked(false);
                isAppSwitchDue = false;
            }

            // Nothing to do if there is no pending event.
            if (!mPendingEvent) {
                return;
            }
        } else {
            // Inbound queue has at least one entry.
            mPendingEvent = mInboundQueue.dequeueAtHead();
        }

        // Get ready to dispatch the event.
        resetANRTimeoutsLocked();
    }

    // Now we have an event to dispatch.
    // All events are eventually dequeued and processed this way, even if we intend to drop them.
    ALOG_ASSERT(mPendingEvent != NULL);
    bool done = false;
    DropReason dropReason = DROP_REASON_NOT_DROPPED;
    if (!(mPendingEvent->policyFlags & POLICY_FLAG_PASS_TO_USER)) {
        dropReason = DROP_REASON_POLICY;
    } else if (!mDispatchEnabled) {
        dropReason = DROP_REASON_DISABLED;
    }

    if (mNextUnblockedEvent == mPendingEvent) {
        mNextUnblockedEvent = NULL;
    }

    switch (mPendingEvent->type) {
    case EventEntry::TYPE_DEVICE_RESET: {
        DeviceResetEntry* typedEntry =
                static_cast<DeviceResetEntry*>(mPendingEvent);
        done = dispatchDeviceResetLocked(typedEntry);
        dropReason = DROP_REASON_NOT_DROPPED; // device resets are never dropped
        break;
    }

    case EventEntry::TYPE_KEY: {
        KeyEntry* typedEntry = static_cast<KeyEntry*>(mPendingEvent);
        if (isAppSwitchDue) {
            if (isAppSwitchKeyEventLocked(typedEntry)) {
                resetPendingAppSwitchLocked(true);
                isAppSwitchDue = false;
            } else if (dropReason == DROP_REASON_NOT_DROPPED) {
                dropReason = DROP_REASON_APP_SWITCH;
            }
        }
        if (dropReason == DROP_REASON_NOT_DROPPED
                && isStaleEventLocked(currentTime, typedEntry)) {
            dropReason = DROP_REASON_STALE;
        }
        if (dropReason == DROP_REASON_NOT_DROPPED && mNextUnblockedEvent) {
            dropReason = DROP_REASON_BLOCKED;
        }
        done = dispatchKeyLocked(currentTime, typedEntry, &dropReason);
        break;
    }

    case EventEntry::TYPE_MOTION: {
        MotionEntry* typedEntry = static_cast<MotionEntry*>(mPendingEvent);
        if (dropReason == DROP_REASON_NOT_DROPPED && isAppSwitchDue) {
            dropReason = DROP_REASON_APP_SWITCH;
        }
        if (dropReason == DROP_REASON_NOT_DROPPED
                && isStaleEventLocked(currentTime, typedEntry)) {
            dropReason = DROP_REASON_STALE;
        }
        if (dropReason == DROP_REASON_NOT_DROPPED && mNextUnblockedEvent) {
            dropReason = DROP_REASON_BLOCKED;
        }
        done = dispatchMotionLocked(currentTime, typedEntry,
                &dropReason);
        break;
    }

    default:
        ALOG_ASSERT(false);
        break;
    }

    if (done) {
        if (dropReason != DROP_REASON_NOT_DROPPED) {
            dropInboundEventLocked(mPendingEvent, dropReason);
        }

        releasePendingEventLocked();
        *nextWakeupTime = LONG_LONG_MIN;  // force next poll to wake up immediately
    }
}

bool MirInputDispatcher::enqueueInboundEventLocked(EventEntry* entry) {
    bool needWake = mInboundQueue.isEmpty();
    mInboundQueue.enqueueAtTail(entry);

    switch (entry->type) {
    case EventEntry::TYPE_KEY: {
        // Optimize app switch latency.
        // If the application takes too long to catch up then we drop all events preceding
        // the app switch key.
        KeyEntry* keyEntry = static_cast<KeyEntry*>(entry);
        if (isAppSwitchKeyEventLocked(keyEntry)) {
            if (keyEntry->action == AKEY_EVENT_ACTION_DOWN) {
                mAppSwitchSawKeyDown = true;
            } else if (keyEntry->action == AKEY_EVENT_ACTION_UP) {
                if (mAppSwitchSawKeyDown) {
#if DEBUG_APP_SWITCH
                    printf("App switch is pending!\n");
#endif
                    mAppSwitchDueTime = keyEntry->eventTime + APP_SWITCH_TIMEOUT;
                    mAppSwitchSawKeyDown = false;
                    needWake = true;
                }
            }
        }
        break;
    }
    }

    return needWake;
}

void MirInputDispatcher::dropInboundEventLocked(EventEntry* entry, DropReason dropReason) {
    const char* reason;
    switch (dropReason) {
    case DROP_REASON_POLICY:
#if DEBUG_INBOUND_EVENT_DETAILS
        printf("Dropped event because policy consumed it.\n");
#endif
        reason = "inbound event was dropped because the policy consumed it";
        break;
    case DROP_REASON_DISABLED:
        ALOGI("Dropped event because input dispatch is disabled.");
        reason = "inbound event was dropped because input dispatch is disabled";
        break;
    case DROP_REASON_APP_SWITCH:
        ALOGI("Dropped event because of pending overdue app switch.");
        reason = "inbound event was dropped because of pending overdue app switch";
        break;
    case DROP_REASON_BLOCKED:
        ALOGI("Dropped event because the current application is not responding and the user "
                "has started interacting with a different application.");
        reason = "inbound event was dropped because the current application is not responding "
                "and the user has started interacting with a different application";
        break;
    case DROP_REASON_STALE:
        ALOGI("Dropped event because it is stale.");
        reason = "inbound event was dropped because it is stale";
        break;
    default:
        ALOG_ASSERT(false);
        return;
    }

    switch (entry->type) {
    case EventEntry::TYPE_KEY: {
        CancelationOptions options(CancelationOptions::CANCEL_NON_POINTER_EVENTS, reason);
        synthesizeCancelationEventsForAllConnectionsLocked(options);
        break;
    }
    case EventEntry::TYPE_MOTION: {
        MotionEntry* motionEntry = static_cast<MotionEntry*>(entry);
        if (motionEntry->source & AINPUT_SOURCE_CLASS_POINTER) {
            CancelationOptions options(CancelationOptions::CANCEL_POINTER_EVENTS, reason);
            synthesizeCancelationEventsForAllConnectionsLocked(options);
        } else {
            CancelationOptions options(CancelationOptions::CANCEL_NON_POINTER_EVENTS, reason);
            synthesizeCancelationEventsForAllConnectionsLocked(options);
        }
        break;
    }
    }
}

bool MirInputDispatcher::isAppSwitchKeyCode(int32_t keyCode) {
    return keyCode == AKEYCODE_HOME || keyCode == AKEYCODE_ENDCALL;
}

bool MirInputDispatcher::isAppSwitchKeyEventLocked(KeyEntry* keyEntry) {
    return ! (keyEntry->flags & AKEY_EVENT_FLAG_CANCELED)
            && isAppSwitchKeyCode(keyEntry->keyCode)
            && (keyEntry->policyFlags & POLICY_FLAG_TRUSTED)
            && (keyEntry->policyFlags & POLICY_FLAG_PASS_TO_USER);
}

bool MirInputDispatcher::isAppSwitchPendingLocked() {
    return mAppSwitchDueTime != LONG_LONG_MAX;
}

void MirInputDispatcher::resetPendingAppSwitchLocked(bool handled) {
    mAppSwitchDueTime = LONG_LONG_MAX;

#if DEBUG_APP_SWITCH
    if (handled) {
        printf("App switch has arrived.\n");
    } else {
        printf("App switch was abandoned.\n");
    }
#else
    (void)handled;
#endif
}

bool MirInputDispatcher::isStaleEventLocked(nsecs_t currentTime, EventEntry* entry) {
    return currentTime - entry->eventTime >= STALE_EVENT_TIMEOUT;
}

bool MirInputDispatcher::runCommandsLockedInterruptible() {
    if (mCommandQueue.isEmpty()) {
        return false;
    }

    do {
        CommandEntry* commandEntry = mCommandQueue.dequeueAtHead();

        Command command = commandEntry->command;
        (this->*command)(commandEntry); // commands are implicitly 'LockedInterruptible'

        commandEntry->connection.clear();
        delete commandEntry;
    } while (! mCommandQueue.isEmpty());
    return true;
}

MirInputDispatcher::CommandEntry* MirInputDispatcher::postCommandLocked(Command command) {
    CommandEntry* commandEntry = new CommandEntry(command);
    mCommandQueue.enqueueAtTail(commandEntry);
    return commandEntry;
}

void MirInputDispatcher::drainInboundQueueLocked() {
    while (! mInboundQueue.isEmpty()) {
        EventEntry* entry = mInboundQueue.dequeueAtHead();
        releaseInboundEventLocked(entry);
    }
}

void MirInputDispatcher::releasePendingEventLocked() {
    if (mPendingEvent) {
        resetANRTimeoutsLocked();
        releaseInboundEventLocked(mPendingEvent);
        mPendingEvent = NULL;
    }
}

void MirInputDispatcher::releaseInboundEventLocked(EventEntry* entry) {
    if (entry == mNextUnblockedEvent) {
        mNextUnblockedEvent = NULL;
    }
    entry->release();
}

bool MirInputDispatcher::dispatchDeviceResetLocked(DeviceResetEntry* entry) {
#if DEBUG_OUTBOUND_EVENT_DETAILS
    printf("dispatchDeviceReset - eventTime=%lld, deviceId=%d\n", entry->eventTime, entry->deviceId);
#endif

    CancelationOptions options(CancelationOptions::CANCEL_ALL_EVENTS,
            "device was reset");
    options.deviceId = entry->deviceId;
    synthesizeCancelationEventsForAllConnectionsLocked(options);
    return true;
}

bool MirInputDispatcher::dispatchKeyLocked(nsecs_t currentTime, KeyEntry* entry,
        DropReason* dropReason) {
    // Preprocessing.
    if (! entry->dispatchInProgress) {
        entry->dispatchInProgress = true;

        logOutboundKeyDetailsLocked("dispatchKey - ", entry);
    }

    // Clean up if dropping the event.
    if (*dropReason != DROP_REASON_NOT_DROPPED) {
        return true;
    }

    // Identify targets.
    Vector<InputTarget> inputTargets;
    //findFocusedWindowTargetsLocked(currentTime,
    //        entry, inputTargets, nextWakeupTime);

    addMonitoringTargetsLocked(inputTargets);

    // Dispatch the key.
    dispatchEventLocked(currentTime, entry, inputTargets);
    return true;
}

void MirInputDispatcher::logOutboundKeyDetailsLocked(const char* prefix, const KeyEntry* entry) {
#if DEBUG_OUTBOUND_EVENT_DETAILS
    printf("%seventTime=%lld, deviceId=%d, source=0x%x, policyFlags=0x%x, "
            "action=0x%x, flags=0x%x, keyCode=0x%x, scanCode=0x%x, metaState=0x%x, "
            "downTime=%lld\n",
            prefix,
            entry->eventTime, entry->deviceId, entry->source, entry->policyFlags,
            entry->action, entry->flags, entry->keyCode, entry->scanCode, entry->metaState,
            entry->downTime);
#else
    (void)prefix;
    (void)entry;
#endif
}

bool MirInputDispatcher::dispatchMotionLocked(
        nsecs_t currentTime, MotionEntry* entry, DropReason* dropReason) {
    // Preprocessing.
    if (! entry->dispatchInProgress) {
        entry->dispatchInProgress = true;

        logOutboundMotionDetailsLocked("dispatchMotion - ", entry);
    }

    // Clean up if dropping the event.
    if (*dropReason != DROP_REASON_NOT_DROPPED) {
#if DEBUG_DISPATCH_CYCLE
        printf("dispatchMotion - returning because of dropReason=%d\n", *dropReason);
#endif
        return true;
    }

    // Identify targets.
    Vector<InputTarget> inputTargets;
    inputTargets.add(entry->target);

    //addMonitoringTargetsLocked(inputTargets);

    // Dispatch the motion.
    dispatchEventLocked(currentTime, entry, inputTargets);
    return true;
}


void MirInputDispatcher::logOutboundMotionDetailsLocked(const char* prefix,
                                                        const MotionEntry* entry) {
#if DEBUG_OUTBOUND_EVENT_DETAILS
    printf("%seventTime=%lld, deviceId=%d, source=0x%x, policyFlags=0x%x, "
            "action=0x%x, flags=0x%x, "
            "metaState=0x%x, buttonState=0x%x, "
            "edgeFlags=0x%x, xPrecision=%f, yPrecision=%f, downTime=%lld\n",
            prefix,
            entry->eventTime, entry->deviceId, entry->source, entry->policyFlags,
            entry->action, entry->flags,
            entry->metaState, entry->buttonState,
            entry->edgeFlags, entry->xPrecision, entry->yPrecision,
            entry->downTime);

    for (uint32_t i = 0; i < entry->pointerCount; i++) {
        printf("  Pointer %d: id=%d, toolType=%d, "
                "x=%f, y=%f, pressure=%f, size=%f, "
                "touchMajor=%f, touchMinor=%f, toolMajor=%f, toolMinor=%f, "
                "orientation=%f\n",
                i, entry->pointerProperties[i].id,
                entry->pointerProperties[i].toolType,
                entry->pointerCoords[i].getAxisValue(AMOTION_EVENT_AXIS_X),
                entry->pointerCoords[i].getAxisValue(AMOTION_EVENT_AXIS_Y),
                entry->pointerCoords[i].getAxisValue(AMOTION_EVENT_AXIS_PRESSURE),
                entry->pointerCoords[i].getAxisValue(AMOTION_EVENT_AXIS_SIZE),
                entry->pointerCoords[i].getAxisValue(AMOTION_EVENT_AXIS_TOUCH_MAJOR),
                entry->pointerCoords[i].getAxisValue(AMOTION_EVENT_AXIS_TOUCH_MINOR),
                entry->pointerCoords[i].getAxisValue(AMOTION_EVENT_AXIS_TOOL_MAJOR),
                entry->pointerCoords[i].getAxisValue(AMOTION_EVENT_AXIS_TOOL_MINOR),
                entry->pointerCoords[i].getAxisValue(AMOTION_EVENT_AXIS_ORIENTATION));
    }
#else
    (void)prefix;
    (void)entry;
#endif
}

void MirInputDispatcher::dispatchEventLocked(nsecs_t currentTime,
        EventEntry* eventEntry, const Vector<InputTarget>& inputTargets) {
#if DEBUG_DISPATCH_CYCLE
    printf("dispatchEventToCurrentInputTargets\n");
#endif

    ALOG_ASSERT(eventEntry->dispatchInProgress); // should already have been set to true

    for (size_t i = 0; i < inputTargets.size(); i++) {
        const InputTarget& inputTarget = inputTargets.itemAt(i);

        ssize_t connectionIndex = getConnectionIndexLocked(inputTarget.inputChannel);
        if (connectionIndex >= 0) {
            sp<Connection> connection = mConnectionsByFd.valueAt(connectionIndex);
            prepareDispatchCycleLocked(currentTime, connection, eventEntry, &inputTarget);
        } else {
#if DEBUG_FOCUS
            printf("Dropping event delivery to target with channel '%s' because it "
                    "is no longer registered with the input dispatcher.\n",
                    c_str(inputTarget.inputChannel->getName()));
#endif
        }
    }
}

void MirInputDispatcher::resetANRTimeoutsLocked() {
#if DEBUG_FOCUS
        printf("Resetting ANR timeouts.\n");
#endif

    // Reset input target wait timeout.
    mInputTargetWaitCause = INPUT_TARGET_WAIT_CAUSE_NONE;
    mInputTargetWaitApplicationHandle.clear();
}

namespace
{
static inline bool targets_contains_channel(Vector<InputTarget> const& input_targets, sp<InputChannel> const& channel)
{
    for (size_t i = 0; i < input_targets.size(); i++)
    {
        auto target = input_targets[i];
        if (target.inputChannel == channel)
            return true;
    }
    return false;
}
}

void MirInputDispatcher::addMonitoringTargetsLocked(Vector<InputTarget>& inputTargets) {
    for (size_t i = 0; i < mMonitoringChannels.size(); i++) {
        if (targets_contains_channel(inputTargets, mMonitoringChannels[i]))
            continue;
        
        inputTargets.push();

        InputTarget& target = inputTargets.editTop();
        target.inputChannel = mMonitoringChannels[i];
        target.flags = InputTarget::FLAG_DISPATCH_AS_IS;
        target.xOffset = 0;
        target.yOffset = 0;
        target.scaleFactor = 1.0f;
    }
}

void MirInputDispatcher::prepareDispatchCycleLocked(nsecs_t currentTime,
        const sp<Connection>& connection, EventEntry* eventEntry, const InputTarget* inputTarget) {
    // Skip this event if the connection status is not normal.
    // We don't want to enqueue additional outbound events if the connection is broken.
    if (connection->status != Connection::STATUS_NORMAL) {
#if DEBUG_DISPATCH_CYCLE
        printf("channel '%s' ~ Dropping event because the channel status is %s\n",
                connection->getInputChannelName(), connection->getStatusLabel());
#endif
        return;
    }

    // Not splitting.  Enqueue dispatch entries for the event as is.
    enqueueDispatchEntriesLocked(currentTime, connection, eventEntry, inputTarget);
}

void MirInputDispatcher::enqueueDispatchEntriesLocked(nsecs_t currentTime,
        const sp<Connection>& connection, EventEntry* eventEntry, const InputTarget* inputTarget) {
    bool wasEmpty = connection->outboundQueue.isEmpty();

    // Enqueue dispatch entries for the requested modes.
    enqueueDispatchEntryLocked(connection, eventEntry, inputTarget,
            InputTarget::FLAG_DISPATCH_AS_HOVER_EXIT);
    enqueueDispatchEntryLocked(connection, eventEntry, inputTarget,
            InputTarget::FLAG_DISPATCH_AS_OUTSIDE);
    enqueueDispatchEntryLocked(connection, eventEntry, inputTarget,
            InputTarget::FLAG_DISPATCH_AS_HOVER_ENTER);
    enqueueDispatchEntryLocked(connection, eventEntry, inputTarget,
            InputTarget::FLAG_DISPATCH_AS_IS);
    enqueueDispatchEntryLocked(connection, eventEntry, inputTarget,
            InputTarget::FLAG_DISPATCH_AS_SLIPPERY_EXIT);
    enqueueDispatchEntryLocked(connection, eventEntry, inputTarget,
            InputTarget::FLAG_DISPATCH_AS_SLIPPERY_ENTER);

    // If the outbound queue was previously empty, start the dispatch cycle going.
    if (wasEmpty && !connection->outboundQueue.isEmpty()) {
        startDispatchCycleLocked(currentTime, connection);
    }
}

void MirInputDispatcher::enqueueDispatchEntryLocked(
        const sp<Connection>& connection, EventEntry* eventEntry, const InputTarget* inputTarget,
        int32_t dispatchMode) {
    int32_t inputTargetFlags = inputTarget->flags;
    if (!(inputTargetFlags & dispatchMode)) {
        return;
    }
    inputTargetFlags = (inputTargetFlags & ~InputTarget::FLAG_DISPATCH_MASK) | dispatchMode;

    // This is a new event.
    // Enqueue a new dispatch entry onto the outbound queue for this connection.
    DispatchEntry* dispatchEntry = new DispatchEntry(eventEntry, // increments ref
            inputTargetFlags, inputTarget->xOffset, inputTarget->yOffset,
            inputTarget->scaleFactor);

    // Apply target flags and update the connection's input state.
    switch (eventEntry->type) {
    case EventEntry::TYPE_KEY: {
        KeyEntry* keyEntry = static_cast<KeyEntry*>(eventEntry);
        dispatchEntry->resolvedAction = keyEntry->action;
        dispatchEntry->resolvedFlags = keyEntry->flags;

        if (!connection->inputState.trackKey(keyEntry,
                dispatchEntry->resolvedAction, dispatchEntry->resolvedFlags)) {
#if DEBUG_DISPATCH_CYCLE
            printf("channel '%s' ~ enqueueDispatchEntryLocked: skipping inconsistent key event\n",
                    connection->getInputChannelName());
#endif
            delete dispatchEntry;
            return; // skip the inconsistent event
        }
        break;
    }

    case EventEntry::TYPE_MOTION: {
        MotionEntry* motionEntry = static_cast<MotionEntry*>(eventEntry);
        if (dispatchMode & InputTarget::FLAG_DISPATCH_AS_OUTSIDE) {
            dispatchEntry->resolvedAction = AMOTION_EVENT_ACTION_OUTSIDE;
        } else if (dispatchMode & InputTarget::FLAG_DISPATCH_AS_HOVER_EXIT) {
            dispatchEntry->resolvedAction = AMOTION_EVENT_ACTION_HOVER_EXIT;
        } else if (dispatchMode & InputTarget::FLAG_DISPATCH_AS_HOVER_ENTER) {
            dispatchEntry->resolvedAction = AMOTION_EVENT_ACTION_HOVER_ENTER;
        } else if (dispatchMode & InputTarget::FLAG_DISPATCH_AS_SLIPPERY_EXIT) {
            dispatchEntry->resolvedAction = AMOTION_EVENT_ACTION_CANCEL;
        } else if (dispatchMode & InputTarget::FLAG_DISPATCH_AS_SLIPPERY_ENTER) {
            dispatchEntry->resolvedAction = AMOTION_EVENT_ACTION_DOWN;
        } else {
            dispatchEntry->resolvedAction = motionEntry->action;
        }
        if (dispatchEntry->resolvedAction == AMOTION_EVENT_ACTION_HOVER_MOVE
                && !connection->inputState.isHovering(
                        motionEntry->deviceId, motionEntry->source)) {
#if DEBUG_DISPATCH_CYCLE
        printf("channel '%s' ~ enqueueDispatchEntryLocked: filling in missing hover enter event\n",
                connection->getInputChannelName());
#endif
            dispatchEntry->resolvedAction = AMOTION_EVENT_ACTION_HOVER_ENTER;
        }

        dispatchEntry->resolvedFlags = motionEntry->flags;
        if (dispatchEntry->targetFlags & InputTarget::FLAG_WINDOW_IS_OBSCURED) {
            dispatchEntry->resolvedFlags |= AMOTION_EVENT_FLAG_WINDOW_IS_OBSCURED;
        }

        if (!connection->inputState.trackMotion(motionEntry,
                dispatchEntry->resolvedAction, dispatchEntry->resolvedFlags)) {
#if DEBUG_DISPATCH_CYCLE
            printf("channel '%s' ~ enqueueDispatchEntryLocked: skipping inconsistent motion event\n",
                    connection->getInputChannelName());
#endif
            delete dispatchEntry;
            return; // skip the inconsistent event
        }
        break;
    }
    }

    // Enqueue the dispatch entry.
    connection->outboundQueue.enqueueAtTail(dispatchEntry);
}

void MirInputDispatcher::startDispatchCycleLocked(nsecs_t currentTime,
        const sp<Connection>& connection) {
#if DEBUG_DISPATCH_CYCLE
    printf("channel '%s' ~ startDispatchCycle\n",
            connection->getInputChannelName());
#endif

    while (connection->status == Connection::STATUS_NORMAL
            && !connection->outboundQueue.isEmpty()) {
        DispatchEntry* dispatchEntry = connection->outboundQueue.head;
        dispatchEntry->deliveryTime = currentTime;

        // Publish the event.
        status_t status;
        EventEntry* eventEntry = dispatchEntry->eventEntry;
        switch (eventEntry->type) {
        case EventEntry::TYPE_KEY: {
            KeyEntry* keyEntry = static_cast<KeyEntry*>(eventEntry);

            // Publish the key event.
            status = connection->inputPublisher.publishKeyEvent(dispatchEntry->seq,
                    keyEntry->deviceId, keyEntry->source,
                    dispatchEntry->resolvedAction, dispatchEntry->resolvedFlags,
                    keyEntry->keyCode, keyEntry->scanCode,
                    keyEntry->metaState, keyEntry->downTime, 0 /*repeatCount*/,
                    keyEntry->eventTime);
            input_report->published_key_event(connection->inputChannel->getFd(),
                                              dispatchEntry->seq,
                                              keyEntry->eventTime);
            break;
        }

        case EventEntry::TYPE_MOTION: {
            MotionEntry* motionEntry = static_cast<MotionEntry*>(eventEntry);

            PointerCoords scaledCoords[MAX_POINTERS];
            const PointerCoords* usingCoords = motionEntry->pointerCoords;

            // Set the X and Y offset depending on the input source.
            float xOffset, yOffset, scaleFactor;
            if ((motionEntry->source & AINPUT_SOURCE_CLASS_POINTER)
                    && !(dispatchEntry->targetFlags & InputTarget::FLAG_ZERO_COORDS)) {
                scaleFactor = dispatchEntry->scaleFactor;
                xOffset = dispatchEntry->xOffset * scaleFactor;
                yOffset = dispatchEntry->yOffset * scaleFactor;
                if (scaleFactor != 1.0f) {
                    for (size_t i = 0; i < motionEntry->pointerCount; i++) {
                        scaledCoords[i] = motionEntry->pointerCoords[i];
                        scaledCoords[i].scale(scaleFactor);
                    }
                    usingCoords = scaledCoords;
                }
            } else {
                xOffset = 0.0f;
                yOffset = 0.0f;
                scaleFactor = 1.0f;

                // We don't want the dispatch target to know.
                if (dispatchEntry->targetFlags & InputTarget::FLAG_ZERO_COORDS) {
                    for (size_t i = 0; i < motionEntry->pointerCount; i++) {
                        scaledCoords[i].clear();
                    }
                    usingCoords = scaledCoords;
                }
            }

            // Publish the motion event.
            qDebug() << "InputDispatcher: publishing motion event";
            status = connection->inputPublisher.publishMotionEvent(dispatchEntry->seq,
                    motionEntry->deviceId, motionEntry->source,
                    dispatchEntry->resolvedAction, dispatchEntry->resolvedFlags,
                    motionEntry->edgeFlags, motionEntry->metaState, motionEntry->buttonState,
                    xOffset, yOffset,
                    motionEntry->xPrecision, motionEntry->yPrecision,
                    motionEntry->downTime, motionEntry->eventTime,
                    motionEntry->pointerCount, motionEntry->pointerProperties,
                    usingCoords);
            input_report->published_motion_event(connection->inputChannel->getFd(),
                                                 dispatchEntry->seq,
                                                 motionEntry->eventTime);
            break;
        }

        default:
            ALOG_ASSERT(false);
            return;
        }

        // Check the result.
        if (status) {
            if (status == WOULD_BLOCK) {
                if (connection->waitQueue.isEmpty()) {
                    ALOGE("channel '%s' ~ Could not publish event because the pipe is full. "
                            "This is unexpected because the wait queue is empty, so the pipe "
                            "should be empty and we shouldn't have any problems writing an "
                            "event to it, status=%d", connection->getInputChannelName(), status);
                    abortBrokenDispatchCycleLocked(connection, true /*notify*/);
                } else {
                    // Pipe is full and we are waiting for the app to finish process some events
                    // before sending more events to it.
#if DEBUG_DISPATCH_CYCLE
                    printf("channel '%s' ~ Could not publish event because the pipe is full, "
                            "waiting for the application to catch up\n",
                            connection->getInputChannelName());
#endif
                    connection->inputPublisherBlocked = true;
                }
            } else {
                ALOGE("channel '%s' ~ Could not publish event due to an unexpected error, "
                        "status=%d", connection->getInputChannelName(), status);
                abortBrokenDispatchCycleLocked(connection, true /*notify*/);
            }
            return;
        }

        // Re-enqueue the event on the wait queue.
        connection->outboundQueue.dequeue(dispatchEntry);
        connection->waitQueue.enqueueAtTail(dispatchEntry);
    }
}

void MirInputDispatcher::finishDispatchCycleLocked(nsecs_t currentTime,
        const sp<Connection>& connection, uint32_t seq, bool handled) {
#if DEBUG_DISPATCH_CYCLE
    printf("channel '%s' ~ finishDispatchCycle - seq=%u, handled=%s\n",
            connection->getInputChannelName(), seq, toString(handled));
#endif

    connection->inputPublisherBlocked = false;

    if (connection->status == Connection::STATUS_BROKEN
            || connection->status == Connection::STATUS_ZOMBIE) {
        return;
    }

    // Notify other system components and prepare to start the next dispatch cycle.
    onDispatchCycleFinishedLocked(currentTime, connection, seq, handled);
}

void MirInputDispatcher::abortBrokenDispatchCycleLocked(const sp<Connection>& connection,
                                                        bool notify) {
#if DEBUG_DISPATCH_CYCLE
    printf("channel '%s' ~ abortBrokenDispatchCycle - notify=%s\n",
            connection->getInputChannelName(), toString(notify));
#endif

    // Clear the dispatch queues.
    drainDispatchQueueLocked(&connection->outboundQueue);
    drainDispatchQueueLocked(&connection->waitQueue);

    // The connection appears to be unrecoverably broken.
    // Ignore already broken or zombie connections.
    if (connection->status == Connection::STATUS_NORMAL) {
        connection->status = Connection::STATUS_BROKEN;

        if (notify) {
            // Notify other system components.
            //onDispatchCycleBrokenLocked(connection);
        }
    }
}

void MirInputDispatcher::drainDispatchQueueLocked(Queue<DispatchEntry>* queue) {
    while (!queue->isEmpty()) {
        DispatchEntry* dispatchEntry = queue->dequeueAtHead();
        releaseDispatchEntryLocked(dispatchEntry);
    }
}

void MirInputDispatcher::releaseDispatchEntryLocked(DispatchEntry* dispatchEntry) {
    delete dispatchEntry;
}

int MirInputDispatcher::handleReceiveCallback(int fd, int events, void* data) {
    MirInputDispatcher* d = static_cast<MirInputDispatcher*>(data);

    { // acquire lock
        AutoMutex _l(d->mLock);

        ssize_t connectionIndex = d->mConnectionsByFd.indexOfKey(fd);
        if (connectionIndex < 0) {
            ALOGE("Received spurious receive callback for unknown input channel.  "
                    "fd=%d, events=0x%x", fd, events);
            return 0; // remove the callback
        }

        bool notify;
        sp<Connection> connection = d->mConnectionsByFd.valueAt(connectionIndex);
        if (!(events & (ALOOPER_EVENT_ERROR | ALOOPER_EVENT_HANGUP))) {
            if (!(events & ALOOPER_EVENT_INPUT)) {
                ALOGW("channel '%s' ~ Received spurious callback for unhandled poll event.  "
                        "events=0x%x", connection->getInputChannelName(), events);
                return 1;
            }

            nsecs_t currentTime = now();
            bool gotOne = false;
            status_t status;
            for (;;) {
                uint32_t seq;
                bool handled;
                status = connection->inputPublisher.receiveFinishedSignal(&seq, &handled);
                if (status) {
                    break;
                }
                d->input_report->received_event_finished_signal(connection->inputChannel->getFd(), seq);
                d->finishDispatchCycleLocked(currentTime, connection, seq, handled);
                gotOne = true;
            }
            if (gotOne) {
                d->runCommandsLockedInterruptible();
                if (status == WOULD_BLOCK) {
                    return 1;
                }
            }

            notify = status != DEAD_OBJECT || !connection->monitor;
            if (notify) {
                ALOGE("channel '%s' ~ Failed to receive finished signal.  status=%d",
                        connection->getInputChannelName(), status);
            }
        } else {
            // Monitor channels are never explicitly unregistered.
            // We do it automatically when the remote endpoint is closed so don't warn
            // about them.
            notify = !connection->monitor;
            if (notify) {
                ALOGW("channel '%s' ~ Consumer closed input channel or an error occurred.  "
                        "events=0x%x", connection->getInputChannelName(), events);
            }
        }

        // Unregister the channel.
        d->unregisterInputChannelLocked(connection->inputChannel, notify);
        return 0; // remove the callback
    } // release lock
}

void MirInputDispatcher::synthesizeCancelationEventsForAllConnectionsLocked(
        const CancelationOptions& options) {
    for (size_t i = 0; i < mConnectionsByFd.size(); i++) {
        synthesizeCancelationEventsForConnectionLocked(
                mConnectionsByFd.valueAt(i), options);
    }
}

void MirInputDispatcher::synthesizeCancelationEventsForInputChannelLocked(
        const sp<InputChannel>& channel, const CancelationOptions& options) {
    ssize_t index = getConnectionIndexLocked(channel);
    if (index >= 0) {
        synthesizeCancelationEventsForConnectionLocked(
                mConnectionsByFd.valueAt(index), options);
    }
}

void MirInputDispatcher::synthesizeCancelationEventsForConnectionLocked(
        const sp<Connection>& connection, const CancelationOptions& options) {
    if (connection->status == Connection::STATUS_BROKEN) {
        return;
    }

    nsecs_t currentTime = now();

    Vector<EventEntry*> cancelationEvents;
    connection->inputState.synthesizeCancelationEvents(currentTime,
            cancelationEvents, options);

    if (!cancelationEvents.isEmpty()) {
#if DEBUG_OUTBOUND_EVENT_DETAILS
        printf("channel '%s' ~ Synthesized %d cancelation events to bring channel back in sync "
                "with reality: %s, mode=%d.\n",
                connection->getInputChannelName(), cancelationEvents.size(),
                options.reason, options.mode);
#endif
        for (size_t i = 0; i < cancelationEvents.size(); i++) {
            EventEntry* cancelationEventEntry = cancelationEvents.itemAt(i);
            switch (cancelationEventEntry->type) {
            case EventEntry::TYPE_KEY:
                logOutboundKeyDetailsLocked("cancel - ",
                        static_cast<KeyEntry*>(cancelationEventEntry));
                break;
            case EventEntry::TYPE_MOTION:
                logOutboundMotionDetailsLocked("cancel - ",
                        static_cast<MotionEntry*>(cancelationEventEntry));
                break;
            }

            InputTarget target;
            target.xOffset = 0;
            target.yOffset = 0;
            target.scaleFactor = 1.0f;
            target.inputChannel = connection->inputChannel;
            target.flags = InputTarget::FLAG_DISPATCH_AS_IS;

            enqueueDispatchEntryLocked(connection, cancelationEventEntry, // increments ref
                    &target, InputTarget::FLAG_DISPATCH_AS_IS);

            cancelationEventEntry->release();
        }

        startDispatchCycleLocked(currentTime, connection);
    }
}

void MirInputDispatcher::notifyKey(const NotifyKeyArgs* args) {
#if DEBUG_INBOUND_EVENT_DETAILS
    printf("notifyKey - eventTime=%lld, deviceId=%d, source=0x%x, policyFlags=0x%x, action=0x%x, "
            "flags=0x%x, keyCode=0x%x, scanCode=0x%x, metaState=0x%x, downTime=%lld\n",
            args->eventTime, args->deviceId, args->source, args->policyFlags,
            args->action, args->flags, args->keyCode, args->scanCode,
            args->metaState, args->downTime);
#endif
    if (!validateKeyEvent(args->action)) {
        return;
    }

    uint32_t policyFlags = args->policyFlags;
    int32_t flags = args->flags;
    int32_t metaState = args->metaState;
    if ((policyFlags & POLICY_FLAG_VIRTUAL) || (flags & AKEY_EVENT_FLAG_VIRTUAL_HARD_KEY)) {
        policyFlags |= POLICY_FLAG_VIRTUAL;
        flags |= AKEY_EVENT_FLAG_VIRTUAL_HARD_KEY;
    }
    if (policyFlags & POLICY_FLAG_ALT) {
        metaState |= AMETA_ALT_ON | AMETA_ALT_LEFT_ON;
    }
    if (policyFlags & POLICY_FLAG_ALT_GR) {
        metaState |= AMETA_ALT_ON | AMETA_ALT_RIGHT_ON;
    }
    if (policyFlags & POLICY_FLAG_SHIFT) {
        metaState |= AMETA_SHIFT_ON | AMETA_SHIFT_LEFT_ON;
    }
    if (policyFlags & POLICY_FLAG_CAPS_LOCK) {
        metaState |= AMETA_CAPS_LOCK_ON;
    }
    if (policyFlags & POLICY_FLAG_FUNCTION) {
        metaState |= AMETA_FUNCTION_ON;
    }

    policyFlags |= POLICY_FLAG_TRUSTED;

    KeyEvent event;
    event.initialize(args->deviceId, args->source, args->action,
            flags, args->keyCode, args->scanCode, metaState, 0,
            args->downTime, args->eventTime);

    if (policyFlags & POLICY_FLAG_WOKE_HERE) {
        flags |= AKEY_EVENT_FLAG_WOKE_HERE;
    }

    bool needWake;
    { // acquire lock
        mLock.lock();

        KeyEntry* newEntry = new KeyEntry(args->eventTime,
                args->deviceId, args->source, policyFlags,
                args->action, flags, args->keyCode, args->scanCode,
                metaState, args->downTime);

        needWake = enqueueInboundEventLocked(newEntry);
        mLock.unlock();
    } // release lock

    if (needWake) {
        mLooper->wake();
    }
}

void MirInputDispatcher::notifyMotion(const NotifyMotionArgs* args,
                                      const InputTarget &target) {
#if DEBUG_INBOUND_EVENT_DETAILS
    printf("notifyMotion - eventTime=%lld, deviceId=%d, source=0x%x, policyFlags=0x%x, "
            "action=0x%x, flags=0x%x, metaState=0x%x, buttonState=0x%x, edgeFlags=0x%x, "
            "xPrecision=%f, yPrecision=%f, downTime=%lld\n",
            args->eventTime, args->deviceId, args->source, args->policyFlags,
            args->action, args->flags, args->metaState, args->buttonState,
            args->edgeFlags, args->xPrecision, args->yPrecision, args->downTime);
    for (uint32_t i = 0; i < args->pointerCount; i++) {
        printf("  Pointer %d: id=%d, toolType=%d, "
                "x=%f, y=%f, pressure=%f, size=%f, "
                "touchMajor=%f, touchMinor=%f, toolMajor=%f, toolMinor=%f, "
                "orientation=%f\n",
                i, args->pointerProperties[i].id,
                args->pointerProperties[i].toolType,
                args->pointerCoords[i].getAxisValue(AMOTION_EVENT_AXIS_X),
                args->pointerCoords[i].getAxisValue(AMOTION_EVENT_AXIS_Y),
                args->pointerCoords[i].getAxisValue(AMOTION_EVENT_AXIS_PRESSURE),
                args->pointerCoords[i].getAxisValue(AMOTION_EVENT_AXIS_SIZE),
                args->pointerCoords[i].getAxisValue(AMOTION_EVENT_AXIS_TOUCH_MAJOR),
                args->pointerCoords[i].getAxisValue(AMOTION_EVENT_AXIS_TOUCH_MINOR),
                args->pointerCoords[i].getAxisValue(AMOTION_EVENT_AXIS_TOOL_MAJOR),
                args->pointerCoords[i].getAxisValue(AMOTION_EVENT_AXIS_TOOL_MINOR),
                args->pointerCoords[i].getAxisValue(AMOTION_EVENT_AXIS_ORIENTATION));
    }
#endif
    if (!validateMotionEvent(args->action, args->pointerCount, args->pointerProperties)) {
        return;
    }

    uint32_t policyFlags = args->policyFlags;
    policyFlags |= POLICY_FLAG_TRUSTED;

    bool needWake;
    { // acquire lock
        mLock.lock();

        // Just enqueue a new motion event.
        MotionEntry* newEntry = new MotionEntry(args->eventTime,
                args->deviceId, args->source, policyFlags,
                args->action, args->flags, args->metaState, args->buttonState,
                args->edgeFlags, args->xPrecision, args->yPrecision, args->downTime,
                args->pointerCount, args->pointerProperties, args->pointerCoords);

        newEntry->target = target;

        needWake = enqueueInboundEventLocked(newEntry);
        mLock.unlock();
    } // release lock

    if (needWake) {
        mLooper->wake();
    }
}

void MirInputDispatcher::notifyDeviceReset(const NotifyDeviceResetArgs* args) {
#if DEBUG_INBOUND_EVENT_DETAILS
    printf("notifyDeviceReset - eventTime=%lld, deviceId=%d\n",
            args->eventTime, args->deviceId);
#endif

    bool needWake;
    { // acquire lock
        AutoMutex _l(mLock);

        DeviceResetEntry* newEntry = new DeviceResetEntry(args->eventTime, args->deviceId);
        needWake = enqueueInboundEventLocked(newEntry);
    } // release lock

    if (needWake) {
        mLooper->wake();
    }
}

void MirInputDispatcher::setInputDispatchMode(bool enabled, bool frozen) {
#if DEBUG_FOCUS
    printf("setInputDispatchMode: enabled=%d, frozen=%d\n", enabled, frozen);
#endif

    bool changed;
    { // acquire lock
        AutoMutex _l(mLock);

        if (mDispatchEnabled != enabled || mDispatchFrozen != frozen) {
            if (mDispatchFrozen && !frozen) {
                resetANRTimeoutsLocked();
            }

            if (mDispatchEnabled && !enabled) {
                resetAndDropEverythingLocked("dispatcher is being disabled");
            }

            mDispatchEnabled = enabled;
            mDispatchFrozen = frozen;
            changed = true;
        } else {
            changed = false;
        }

#if DEBUG_FOCUS
        //logDispatchStateLocked();
#endif
    } // release lock

    if (changed) {
        // Wake up poll loop since it may need to make new input dispatching choices.
        mLooper->wake();
    }
}

void MirInputDispatcher::resetAndDropEverythingLocked(const char* reason) {
#if DEBUG_FOCUS
    printf("Resetting and dropping all events (%s).\n", reason);
#endif

    CancelationOptions options(CancelationOptions::CANCEL_ALL_EVENTS, reason);
    synthesizeCancelationEventsForAllConnectionsLocked(options);

    releasePendingEventLocked();
    drainInboundQueueLocked();
    resetANRTimeoutsLocked();
}

void MirInputDispatcher::logDispatchStateLocked() {
    String8 dump;
    dumpDispatchStateLocked(dump);

    char* text = lockBuffer(dump, dump.size());
    char* start = text;
    while (*start != '\0') {
        char* end = strchr(start, '\n');
        if (*end == '\n') {
            *(end++) = '\0';
        }
        printf("%s\n", start);
        start = end;
    }
}

void MirInputDispatcher::dumpDispatchStateLocked(String8& dump) {
    appendFormat(dump, INDENT "DispatchEnabled: %d\n", mDispatchEnabled);
    appendFormat(dump, INDENT "DispatchFrozen: %d\n", mDispatchFrozen);

    dump.append(INDENT "Windows:\n");

    if (!mMonitoringChannels.isEmpty()) {
        dump.append(INDENT "MonitoringChannels:\n");
        for (size_t i = 0; i < mMonitoringChannels.size(); i++) {
            const sp<InputChannel>& channel = mMonitoringChannels[i];
            appendFormat(dump, INDENT2 "%d: '%s'\n", i, c_str(channel->getName()));
        }
    } else {
        dump.append(INDENT "MonitoringChannels: <none>\n");
    }

    nsecs_t currentTime = now();

    if (!mInboundQueue.isEmpty()) {
        appendFormat(dump, INDENT "InboundQueue: length=%u\n", mInboundQueue.count());
        for (EventEntry* entry = mInboundQueue.head; entry; entry = entry->next) {
            dump.append(INDENT2);
            entry->appendDescription(dump);
            appendFormat(dump, ", age=%0.1fms\n",
                    (currentTime - entry->eventTime) * 0.000001f);
        }
    } else {
        dump.append(INDENT "InboundQueue: <empty>\n");
    }

    if (!mConnectionsByFd.isEmpty()) {
        dump.append(INDENT "Connections:\n");
        for (size_t i = 0; i < mConnectionsByFd.size(); i++) {
            const sp<Connection>& connection = mConnectionsByFd.valueAt(i);
            appendFormat(dump, INDENT2 "%d: channelName='%s', windowName='%s', "
                    "status=%s, monitor=%s, inputPublisherBlocked=%s\n",
                    i, connection->getInputChannelName(), connection->getWindowName(),
                    connection->getStatusLabel(), toString(connection->monitor),
                    toString(connection->inputPublisherBlocked));

            if (!connection->outboundQueue.isEmpty()) {
                appendFormat(dump, INDENT3 "OutboundQueue: length=%u\n",
                        connection->outboundQueue.count());
                for (DispatchEntry* entry = connection->outboundQueue.head; entry;
                        entry = entry->next) {
                    dump.append(INDENT4);
                    entry->eventEntry->appendDescription(dump);
                    appendFormat(dump, ", targetFlags=0x%08x, resolvedAction=%d, age=%0.1fms\n",
                            entry->targetFlags, entry->resolvedAction,
                            (currentTime - entry->eventEntry->eventTime) * 0.000001f);
                }
            } else {
                dump.append(INDENT3 "OutboundQueue: <empty>\n");
            }

            if (!connection->waitQueue.isEmpty()) {
                appendFormat(dump, INDENT3 "WaitQueue: length=%u\n",
                        connection->waitQueue.count());
                for (DispatchEntry* entry = connection->waitQueue.head; entry;
                        entry = entry->next) {
                    dump.append(INDENT4);
                    entry->eventEntry->appendDescription(dump);
                    appendFormat(dump, ", targetFlags=0x%08x, resolvedAction=%d, "
                            "age=%0.1fms, wait=%0.1fms\n",
                            entry->targetFlags, entry->resolvedAction,
                            (currentTime - entry->eventEntry->eventTime) * 0.000001f,
                            (currentTime - entry->deliveryTime) * 0.000001f);
                }
            } else {
                dump.append(INDENT3 "WaitQueue: <empty>\n");
            }
        }
    } else {
        dump.append(INDENT "Connections: <none>\n");
    }

    if (isAppSwitchPendingLocked()) {
        appendFormat(dump, INDENT "AppSwitch: pending, due in %0.1fms\n",
                (mAppSwitchDueTime - now()) / 1000000.0);
    } else {
        dump.append(INDENT "AppSwitch: not pending\n");
    }
}

status_t MirInputDispatcher::registerInputChannel(const sp<InputChannel>& inputChannel, bool monitor) {
#if DEBUG_REGISTRATION
    printf("channel '%s' ~ registerInputChannel - monitor=%s\n", c_str(inputChannel->getName()),
            toString(monitor));
#endif

    { // acquire lock
        AutoMutex _l(mLock);

        if (getConnectionIndexLocked(inputChannel) >= 0) {
            ALOGW("Attempted to register already registered input channel '%s'\n",
                c_str(inputChannel->getName()));
            return BAD_VALUE;
        }

        sp<Connection> connection = new Connection(inputChannel, monitor);

        int fd = inputChannel->getFd();
        mConnectionsByFd.add(fd, connection);

        if (monitor) {
            mMonitoringChannels.push(inputChannel);
        }

        mLooper->addFd(fd, 0, ALOOPER_EVENT_INPUT, handleReceiveCallback, this);

        runCommandsLockedInterruptible();
    } // release lock
    return OK;
}

status_t MirInputDispatcher::unregisterInputChannel(const sp<InputChannel>& inputChannel) {
#if DEBUG_REGISTRATION
    printf("channel '%s' ~ unregisterInputChannel\n", c_str(inputChannel->getName()));
#endif

    { // acquire lock
        AutoMutex _l(mLock);

        status_t status = unregisterInputChannelLocked(inputChannel, false /*notify*/);
        if (status) {
            return status;
        }
    } // release lock

    // Wake the poll loop because removing the connection may have changed the current
    // synchronization state.
    mLooper->wake();
    return OK;
}

status_t MirInputDispatcher::unregisterInputChannelLocked(const sp<InputChannel>& inputChannel,
        bool notify) {
    ssize_t connectionIndex = getConnectionIndexLocked(inputChannel);
    if (connectionIndex < 0) {
        ALOGW("Attempted to unregister already unregistered input channel '%s'",
            c_str(inputChannel->getName()));
        return BAD_VALUE;
    }

    sp<Connection> connection = mConnectionsByFd.valueAt(connectionIndex);
    mConnectionsByFd.removeItemsAt(connectionIndex);

    if (connection->monitor) {
        removeMonitorChannelLocked(inputChannel);
    }

    mLooper->removeFd(inputChannel->getFd());

    abortBrokenDispatchCycleLocked(connection, notify);

    runCommandsLockedInterruptible();

    connection->status = Connection::STATUS_ZOMBIE;
    return OK;
}

void MirInputDispatcher::removeMonitorChannelLocked(const sp<InputChannel>& inputChannel) {
    for (size_t i = 0; i < mMonitoringChannels.size(); i++) {
         if (mMonitoringChannels[i] == inputChannel) {
             mMonitoringChannels.removeAt(i);
             break;
         }
    }
}

ssize_t MirInputDispatcher::getConnectionIndexLocked(const sp<InputChannel>& inputChannel) {
    ssize_t connectionIndex = mConnectionsByFd.indexOfKey(inputChannel->getFd());
    if (connectionIndex >= 0) {
        sp<Connection> connection = mConnectionsByFd.valueAt(connectionIndex);
        if (connection->inputChannel.get() == inputChannel.get()) {
            return connectionIndex;
        }
    }

    return -1;
}

void MirInputDispatcher::onDispatchCycleFinishedLocked(
        nsecs_t currentTime, const sp<Connection>& connection, uint32_t seq, bool handled) {
    CommandEntry* commandEntry = postCommandLocked(
            & MirInputDispatcher::doDispatchCycleFinishedLockedInterruptible);
    commandEntry->connection = connection;
    commandEntry->eventTime = currentTime;
    commandEntry->seq = seq;
    commandEntry->handled = handled;
}

/*
void MirInputDispatcher::onDispatchCycleBrokenLocked(const sp<Connection>& connection) {
    ALOGE("channel '%s' ~ Channel is unrecoverably broken and will be disposed!",
            connection->getInputChannelName());

    CommandEntry* commandEntry = postCommandLocked(
            & MirInputDispatcher::doNotifyInputChannelBrokenLockedInterruptible);
    commandEntry->connection = connection;
}
*/


/*
void MirInputDispatcher::doNotifyInputChannelBrokenLockedInterruptible(
        CommandEntry* commandEntry) {
    sp<Connection> connection = commandEntry->connection;

    if (connection->status != Connection::STATUS_ZOMBIE) {
        mLock.unlock();

        mPolicy->notifyInputChannelBroken(connection->inputWindowHandle);

        mLock.lock();
    }
}
*/

void MirInputDispatcher::doDispatchCycleFinishedLockedInterruptible(
        CommandEntry* commandEntry) {
    sp<Connection> connection = commandEntry->connection;
    nsecs_t finishTime = commandEntry->eventTime;
    uint32_t seq = commandEntry->seq;
    bool handled = commandEntry->handled;

    // Handle post-event policy actions.
    DispatchEntry* dispatchEntry = connection->findWaitQueueEntry(seq);
    if (dispatchEntry) {
        nsecs_t eventDuration = finishTime - dispatchEntry->deliveryTime;
        if (eventDuration > SLOW_EVENT_PROCESSING_WARNING_TIMEOUT) {
            String8 msg;
            appendFormat(msg, "Window '%s' spent %0.1fms processing the last input event: ",
                    connection->getWindowName(), eventDuration * 0.000001f);
            dispatchEntry->eventEntry->appendDescription(msg);
            ALOGI("%s", c_str(msg));
        }

        bool restartEvent;
        if (dispatchEntry->eventEntry->type == EventEntry::TYPE_KEY) {
            KeyEntry* keyEntry = static_cast<KeyEntry*>(dispatchEntry->eventEntry);
            restartEvent = afterKeyEventLockedInterruptible(connection,
                    dispatchEntry, keyEntry, handled);
        } else {
            restartEvent = false;
        }

        // Dequeue the event and start the next cycle.
        // Note that because the lock might have been released, it is possible that the
        // contents of the wait queue to have been drained, so we need to double-check
        // a few things.
        if (dispatchEntry == connection->findWaitQueueEntry(seq)) {
            connection->waitQueue.dequeue(dispatchEntry);
            if (restartEvent && connection->status == Connection::STATUS_NORMAL) {
                connection->outboundQueue.enqueueAtHead(dispatchEntry);
            } else {
                releaseDispatchEntryLocked(dispatchEntry);
            }
        }

        // Start the next dispatch cycle for this connection.
        startDispatchCycleLocked(now(), connection);
    }
}

bool MirInputDispatcher::afterKeyEventLockedInterruptible(const sp<Connection>& connection,
        DispatchEntry* dispatchEntry, KeyEntry* keyEntry, bool handled) {
    if (!(keyEntry->flags & AKEY_EVENT_FLAG_FALLBACK)) {
        // Get the fallback key state.
        // Clear it out after dispatching the UP.
        int32_t originalKeyCode = keyEntry->keyCode;
        int32_t fallbackKeyCode = connection->inputState.getFallbackKey(originalKeyCode);
        if (keyEntry->action == AKEY_EVENT_ACTION_UP) {
            connection->inputState.removeFallbackKey(originalKeyCode);
        }

        if (handled || !dispatchEntry->hasForegroundTarget()) {
            // If the application handles the original key for which we previously
            // generated a fallback or if the window is not a foreground window,
            // then cancel the associated fallback key, if any.
            if (fallbackKeyCode != -1) {
                // Dispatch the unhandled key to the policy with the cancel flag.
#if DEBUG_OUTBOUND_EVENT_DETAILS
                printf("Unhandled key event: Asking policy to cancel fallback action.  "
                        "keyCode=%d, action=%d, policyFlags=0x%08x\n",
                        keyEntry->keyCode, keyEntry->action, keyEntry->policyFlags);
#endif
                KeyEvent event;
                initializeKeyEvent(&event, keyEntry);
                event.setFlags(event.getFlags() | AKEY_EVENT_FLAG_CANCELED);

                // Cancel the fallback key.
                if (fallbackKeyCode != AKEYCODE_UNKNOWN) {
                    CancelationOptions options(CancelationOptions::CANCEL_FALLBACK_EVENTS,
                            "application handled the original non-fallback key "
                            "or is no longer a foreground target, "
                            "canceling previously dispatched fallback key");
                    options.keyCode = fallbackKeyCode;
                    synthesizeCancelationEventsForConnectionLocked(connection, options);
                }
                connection->inputState.removeFallbackKey(originalKeyCode);
            }
        } else {
            // If the application did not handle a non-fallback key, first check
            // that we are in a good state to perform unhandled key event processing
            // Then ask the policy what to do with it.
            bool initialDown = keyEntry->action == AKEY_EVENT_ACTION_DOWN;
            if (fallbackKeyCode == -1 && !initialDown) {
#if DEBUG_OUTBOUND_EVENT_DETAILS
                printf("Unhandled key event: Skipping unhandled key event processing "
                        "since this is not an initial down.  "
                        "keyCode=%d, action=%d, policyFlags=0x%08x\n",
                        originalKeyCode, keyEntry->action, keyEntry->policyFlags);
#endif
                return false;
            }

            // Dispatch the unhandled key to the policy.
#if DEBUG_OUTBOUND_EVENT_DETAILS
            printf("Unhandled key event: Asking policy to perform fallback action.  "
                    "keyCode=%d, action=%d, policyFlags=0x%08x\n",
                    keyEntry->keyCode, keyEntry->action, keyEntry->policyFlags);
#endif
            KeyEvent event;
            initializeKeyEvent(&event, keyEntry);

            // fallback = mPolicy->dispatchUnhandledKey
            bool fallback = false;

            if (connection->status != Connection::STATUS_NORMAL) {
                connection->inputState.removeFallbackKey(originalKeyCode);
                return false;
            }

            // Latch the fallback keycode for this key on an initial down.
            // The fallback keycode cannot change at any other point in the lifecycle.
            if (initialDown) {
                if (fallback) {
                    fallbackKeyCode = event.getKeyCode();
                } else {
                    fallbackKeyCode = AKEYCODE_UNKNOWN;
                }
                connection->inputState.setFallbackKey(originalKeyCode, fallbackKeyCode);
            }

            ALOG_ASSERT(fallbackKeyCode != -1);

            // Cancel the fallback key if the policy decides not to send it anymore.
            // We will continue to dispatch the key to the policy but we will no
            // longer dispatch a fallback key to the application.
            if (fallbackKeyCode != AKEYCODE_UNKNOWN
                    && (!fallback || fallbackKeyCode != event.getKeyCode())) {
#if DEBUG_OUTBOUND_EVENT_DETAILS
                if (fallback) {
                    printf("Unhandled key event: Policy requested to send key %d"
                            "as a fallback for %d, but on the DOWN it had requested "
                            "to send %d instead.  Fallback canceled.\n",
                            event.getKeyCode(), originalKeyCode, fallbackKeyCode);
                } else {
                    printf("Unhandled key event: Policy did not request fallback for %d, "
                            "but on the DOWN it had requested to send %d.  "
                            "Fallback canceled.\n",
                            originalKeyCode, fallbackKeyCode);
                }
#endif

                CancelationOptions options(CancelationOptions::CANCEL_FALLBACK_EVENTS,
                        "canceling fallback, policy no longer desires it");
                options.keyCode = fallbackKeyCode;
                synthesizeCancelationEventsForConnectionLocked(connection, options);

                fallback = false;
                fallbackKeyCode = AKEYCODE_UNKNOWN;
                if (keyEntry->action != AKEY_EVENT_ACTION_UP) {
                    connection->inputState.setFallbackKey(originalKeyCode,
                            fallbackKeyCode);
                }
            }

#if DEBUG_OUTBOUND_EVENT_DETAILS
            {
                String8 msg;
                const KeyedVector<int32_t, int32_t>& fallbackKeys =
                        connection->inputState.getFallbackKeys();
                for (size_t i = 0; i < fallbackKeys.size(); i++) {
                    appendFormat(msg, ", %d->%d", fallbackKeys.keyAt(i),
                            fallbackKeys.valueAt(i));
                }
                printf("Unhandled key event: %d currently tracked fallback keys%s.\n",
                        fallbackKeys.size(), c_str(msg));
            }
#endif

            if (fallback) {
                // Restart the dispatch cycle using the fallback key.
                keyEntry->eventTime = event.getEventTime();
                keyEntry->deviceId = event.getDeviceId();
                keyEntry->source = event.getSource();
                keyEntry->flags = event.getFlags() | AKEY_EVENT_FLAG_FALLBACK;
                keyEntry->keyCode = fallbackKeyCode;
                keyEntry->scanCode = event.getScanCode();
                keyEntry->metaState = event.getMetaState();
                keyEntry->downTime = event.getDownTime();

#if DEBUG_OUTBOUND_EVENT_DETAILS
                printf("Unhandled key event: Dispatching fallback key.  "
                        "originalKeyCode=%d, fallbackKeyCode=%d, fallbackMetaState=%08x\n",
                        originalKeyCode, fallbackKeyCode, keyEntry->metaState);
#endif
                return true; // restart the event
            } else {
#if DEBUG_OUTBOUND_EVENT_DETAILS
                printf("Unhandled key event: No fallback key.\n");
#endif
            }
        }
    }
    return false;
}

void MirInputDispatcher::initializeKeyEvent(KeyEvent* event, const KeyEntry* entry) {
    event->initialize(entry->deviceId, entry->source, entry->action, entry->flags,
            entry->keyCode, entry->scanCode, entry->metaState, 0 /*repeatCount*/,
            entry->downTime, entry->eventTime);
}

void MirInputDispatcher::dump(String8& dump) {
    AutoMutex _l(mLock);

    dump.append("Input Dispatcher State:\n");
    dumpDispatchStateLocked(dump);

    if (!isEmpty(mLastANRState)) {
        dump.append("\nInput Dispatcher State at time of last ANR:\n");
        dump.append(mLastANRState);
    }
}

void MirInputDispatcher::monitor() {
    // Acquire and release the lock to ensure that the dispatcher has not deadlocked.
    mLock.lock();
    mLooper->wake();
    mDispatcherIsAliveCondition.wait(mLock);
    mLock.unlock();
}

// --- MirInputDispatcher::Queue ---

template <typename T>
uint32_t MirInputDispatcher::Queue<T>::count() const {
    uint32_t result = 0;
    for (const T* entry = head; entry; entry = entry->next) {
        result += 1;
    }
    return result;
}

// --- MirInputDispatcher::EventEntry ---

MirInputDispatcher::EventEntry::EventEntry(int32_t type, nsecs_t eventTime, uint32_t policyFlags) :
        refCount(1), type(type), eventTime(eventTime), policyFlags(policyFlags),
        dispatchInProgress(false) {
}

MirInputDispatcher::EventEntry::~EventEntry() {
}

void MirInputDispatcher::EventEntry::release() {
    refCount -= 1;
    if (refCount == 0) {
        delete this;
    } else {
        ALOG_ASSERT(refCount > 0);
    }
}

// --- MirInputDispatcher::DeviceResetEntry ---

MirInputDispatcher::DeviceResetEntry::DeviceResetEntry(nsecs_t eventTime, int32_t deviceId) :
        EventEntry(TYPE_DEVICE_RESET, eventTime, 0),
        deviceId(deviceId) {
}

MirInputDispatcher::DeviceResetEntry::~DeviceResetEntry() {
}

void MirInputDispatcher::DeviceResetEntry::appendDescription(String8& msg) const {
    appendFormat(msg, "DeviceResetEvent(deviceId=%d)", deviceId);
}


// --- MirInputDispatcher::KeyEntry ---

MirInputDispatcher::KeyEntry::KeyEntry(nsecs_t eventTime,
        int32_t deviceId, uint32_t source, uint32_t policyFlags, int32_t action,
        int32_t flags, int32_t keyCode, int32_t scanCode, int32_t metaState,
        nsecs_t downTime) :
        EventEntry(TYPE_KEY, eventTime, policyFlags),
        deviceId(deviceId), source(source), action(action), flags(flags),
        keyCode(keyCode), scanCode(scanCode), metaState(metaState),
        downTime(downTime) {
}

MirInputDispatcher::KeyEntry::~KeyEntry() {
}

void MirInputDispatcher::KeyEntry::appendDescription(String8& msg) const {
    appendFormat(msg, "KeyEvent(action=%d, deviceId=%d, source=0x%08x)",
            action, deviceId, source);
}

void MirInputDispatcher::KeyEntry::recycle() {
    dispatchInProgress = false;
}


// --- MirInputDispatcher::MotionEntry ---

MirInputDispatcher::MotionEntry::MotionEntry(nsecs_t eventTime,
        int32_t deviceId, uint32_t source, uint32_t policyFlags, int32_t action, int32_t flags,
        int32_t metaState, int32_t buttonState,
        int32_t edgeFlags, float xPrecision, float yPrecision,
        nsecs_t downTime, uint32_t pointerCount,
        const PointerProperties* pointerProperties, const PointerCoords* pointerCoords) :
        EventEntry(TYPE_MOTION, eventTime, policyFlags),
        eventTime(eventTime),
        deviceId(deviceId), source(source), action(action), flags(flags),
        metaState(metaState), buttonState(buttonState), edgeFlags(edgeFlags),
        xPrecision(xPrecision), yPrecision(yPrecision),
        downTime(downTime), pointerCount(pointerCount) {
    for (uint32_t i = 0; i < pointerCount; i++) {
        this->pointerProperties[i].copyFrom(pointerProperties[i]);
        this->pointerCoords[i].copyFrom(pointerCoords[i]);
    }
}

MirInputDispatcher::MotionEntry::~MotionEntry() {
}

void MirInputDispatcher::MotionEntry::appendDescription(String8& msg) const {
    appendFormat(msg, "MotionEvent(action=%d, deviceId=%d, source=0x%08x)",
            action, deviceId, source);
}


// --- MirInputDispatcher::DispatchEntry ---

android_atomic_int32_t MirInputDispatcher::DispatchEntry::sNextSeqAtomic;

MirInputDispatcher::DispatchEntry::DispatchEntry(EventEntry* eventEntry,
        int32_t targetFlags, float xOffset, float yOffset, float scaleFactor) :
        seq(nextSeq()),
        eventEntry(eventEntry), targetFlags(targetFlags),
        xOffset(xOffset), yOffset(yOffset), scaleFactor(scaleFactor),
        deliveryTime(0), resolvedAction(0), resolvedFlags(0) {
    eventEntry->refCount += 1;
}

MirInputDispatcher::DispatchEntry::~DispatchEntry() {
    eventEntry->release();
}

uint32_t MirInputDispatcher::DispatchEntry::nextSeq() {
    // Sequence number 0 is reserved and will never be returned.
    uint32_t seq;
    do {
        seq = android_atomic_inc(&sNextSeqAtomic);
    } while (!seq);
    return seq;
}


// --- MirInputDispatcher::InputState ---

MirInputDispatcher::InputState::InputState() {
}

MirInputDispatcher::InputState::~InputState() {
}

bool MirInputDispatcher::InputState::isNeutral() const {
    return mKeyMementos.isEmpty() && mMotionMementos.isEmpty();
}

bool MirInputDispatcher::InputState::isHovering(int32_t deviceId, uint32_t source) const {
    for (size_t i = 0; i < mMotionMementos.size(); i++) {
        const MotionMemento& memento = mMotionMementos.itemAt(i);
        if (memento.deviceId == deviceId
                && memento.source == source
                && memento.hovering) {
            return true;
        }
    }
    return false;
}

bool MirInputDispatcher::InputState::trackKey(const KeyEntry* entry,
        int32_t action, int32_t flags) {
    switch (action) {
    case AKEY_EVENT_ACTION_UP: {
        if (entry->flags & AKEY_EVENT_FLAG_FALLBACK) {
            for (size_t i = 0; i < mFallbackKeys.size(); ) {
                if (mFallbackKeys.valueAt(i) == entry->keyCode) {
                    mFallbackKeys.removeItemsAt(i);
                } else {
                    i += 1;
                }
            }
        }
        ssize_t index = findKeyMemento(entry);
        if (index >= 0) {
            mKeyMementos.removeAt(index);
            return true;
        }
        /* FIXME: We can't just drop the key up event because that prevents creating
         * popup windows that are automatically shown when a key is held and then
         * dismissed when the key is released.  The problem is that the popup will
         * not have received the original key down, so the key up will be considered
         * to be inconsistent with its observed state.  We could perhaps handle this
         * by synthesizing a key down but that will cause other problems.
         *
         * So for now, allow inconsistent key up events to be dispatched.
         *
#if DEBUG_OUTBOUND_EVENT_DETAILS
        printf("Dropping inconsistent key up event: deviceId=%d, source=%08x, "
                "keyCode=%d, scanCode=%d",
                entry->deviceId, entry->source, entry->keyCode, entry->scanCode);
#endif
        return false;
        */
        return true;
    }

    case AKEY_EVENT_ACTION_DOWN: {
        ssize_t index = findKeyMemento(entry);
        if (index >= 0) {
            mKeyMementos.removeAt(index);
        }
        addKeyMemento(entry, flags);
        return true;
    }

    default:
        return true;
    }
}

bool MirInputDispatcher::InputState::trackMotion(const MotionEntry* entry,
        int32_t action, int32_t flags) {
    int32_t actionMasked = action & AMOTION_EVENT_ACTION_MASK;
    switch (actionMasked) {
    case AMOTION_EVENT_ACTION_UP:
    case AMOTION_EVENT_ACTION_CANCEL: {
        ssize_t index = findMotionMemento(entry, false /*hovering*/);
        if (index >= 0) {
            mMotionMementos.removeAt(index);
            return true;
        }
#if DEBUG_OUTBOUND_EVENT_DETAILS
        printf("Dropping inconsistent motion up or cancel event: deviceId=%d, source=%08x, "
                "actionMasked=%d\n",
                entry->deviceId, entry->source, actionMasked);
#endif
        return false;
    }

    case AMOTION_EVENT_ACTION_DOWN: {
        ssize_t index = findMotionMemento(entry, false /*hovering*/);
        if (index >= 0) {
            mMotionMementos.removeAt(index);
        }
        addMotionMemento(entry, flags, false /*hovering*/);
        return true;
    }

    case AMOTION_EVENT_ACTION_POINTER_UP:
    case AMOTION_EVENT_ACTION_POINTER_DOWN:
    case AMOTION_EVENT_ACTION_MOVE: {
        ssize_t index = findMotionMemento(entry, false /*hovering*/);
        if (index >= 0) {
            MotionMemento& memento = mMotionMementos.editItemAt(index);
            memento.setPointers(entry);
            return true;
        }
        if (actionMasked == AMOTION_EVENT_ACTION_MOVE
                && (entry->source & (AINPUT_SOURCE_CLASS_JOYSTICK
                        | AINPUT_SOURCE_CLASS_NAVIGATION))) {
            // Joysticks and trackballs can send MOVE events without corresponding DOWN or UP.
            return true;
        }
#if DEBUG_OUTBOUND_EVENT_DETAILS
        printf("Dropping inconsistent motion pointer up/down or move event: "
                "deviceId=%d, source=%08x, actionMasked=%d\n",
                entry->deviceId, entry->source, actionMasked);
#endif
        return false;
    }

    case AMOTION_EVENT_ACTION_HOVER_EXIT: {
        ssize_t index = findMotionMemento(entry, true /*hovering*/);
        if (index >= 0) {
            mMotionMementos.removeAt(index);
            return true;
        }
#if DEBUG_OUTBOUND_EVENT_DETAILS
        printf("Dropping inconsistent motion hover exit event: deviceId=%d, source=%08x\n",
                entry->deviceId, entry->source);
#endif
        return false;
    }

    case AMOTION_EVENT_ACTION_HOVER_ENTER:
    case AMOTION_EVENT_ACTION_HOVER_MOVE: {
        ssize_t index = findMotionMemento(entry, true /*hovering*/);
        if (index >= 0) {
            mMotionMementos.removeAt(index);
        }
        addMotionMemento(entry, flags, true /*hovering*/);
        return true;
    }

    default:
        return true;
    }
}

ssize_t MirInputDispatcher::InputState::findKeyMemento(const KeyEntry* entry) const {
    for (size_t i = 0; i < mKeyMementos.size(); i++) {
        const KeyMemento& memento = mKeyMementos.itemAt(i);
        if (memento.deviceId == entry->deviceId
                && memento.source == entry->source
                && memento.keyCode == entry->keyCode
                && memento.scanCode == entry->scanCode) {
            return i;
        }
    }
    return -1;
}

ssize_t MirInputDispatcher::InputState::findMotionMemento(const MotionEntry* entry,
        bool hovering) const {
    for (size_t i = 0; i < mMotionMementos.size(); i++) {
        const MotionMemento& memento = mMotionMementos.itemAt(i);
        if (memento.deviceId == entry->deviceId
                && memento.source == entry->source
                && memento.hovering == hovering) {
            return i;
        }
    }
    return -1;
}

void MirInputDispatcher::InputState::addKeyMemento(const KeyEntry* entry, int32_t flags) {
    mKeyMementos.push();
    KeyMemento& memento = mKeyMementos.editTop();
    memento.deviceId = entry->deviceId;
    memento.source = entry->source;
    memento.keyCode = entry->keyCode;
    memento.scanCode = entry->scanCode;
    memento.metaState = entry->metaState;
    memento.flags = flags;
    memento.downTime = entry->downTime;
    memento.policyFlags = entry->policyFlags;
}

void MirInputDispatcher::InputState::addMotionMemento(const MotionEntry* entry,
        int32_t flags, bool hovering) {
    mMotionMementos.push();
    MotionMemento& memento = mMotionMementos.editTop();
    memento.deviceId = entry->deviceId;
    memento.source = entry->source;
    memento.flags = flags;
    memento.xPrecision = entry->xPrecision;
    memento.yPrecision = entry->yPrecision;
    memento.downTime = entry->downTime;
    memento.setPointers(entry);
    memento.hovering = hovering;
    memento.policyFlags = entry->policyFlags;
}

void MirInputDispatcher::InputState::MotionMemento::setPointers(const MotionEntry* entry) {
    pointerCount = entry->pointerCount;
    for (uint32_t i = 0; i < entry->pointerCount; i++) {
        pointerProperties[i].copyFrom(entry->pointerProperties[i]);
        pointerCoords[i].copyFrom(entry->pointerCoords[i]);
    }
}

void MirInputDispatcher::InputState::synthesizeCancelationEvents(nsecs_t currentTime,
        Vector<EventEntry*>& outEvents, const CancelationOptions& options) {
    for (size_t i = 0; i < mKeyMementos.size(); i++) {
        const KeyMemento& memento = mKeyMementos.itemAt(i);
        if (shouldCancelKey(memento, options)) {
            outEvents.push(new KeyEntry(currentTime,
                    memento.deviceId, memento.source, memento.policyFlags,
                    AKEY_EVENT_ACTION_UP, memento.flags | AKEY_EVENT_FLAG_CANCELED,
                    memento.keyCode, memento.scanCode, memento.metaState, memento.downTime));
        }
    }

    for (size_t i = 0; i < mMotionMementos.size(); i++) {
        const MotionMemento& memento = mMotionMementos.itemAt(i);
        if (shouldCancelMotion(memento, options)) {
            outEvents.push(new MotionEntry(currentTime,
                    memento.deviceId, memento.source, memento.policyFlags,
                    memento.hovering
                            ? AMOTION_EVENT_ACTION_HOVER_EXIT
                            : AMOTION_EVENT_ACTION_CANCEL,
                    memento.flags, 0, 0, 0,
                    memento.xPrecision, memento.yPrecision, memento.downTime,
                    memento.pointerCount, memento.pointerProperties, memento.pointerCoords));
        }
    }
}

void MirInputDispatcher::InputState::clear() {
    mKeyMementos.clear();
    mMotionMementos.clear();
    mFallbackKeys.clear();
}

void MirInputDispatcher::InputState::copyPointerStateTo(InputState& other) const {
    for (size_t i = 0; i < mMotionMementos.size(); i++) {
        const MotionMemento& memento = mMotionMementos.itemAt(i);
        if (memento.source & AINPUT_SOURCE_CLASS_POINTER) {
            for (size_t j = 0; j < other.mMotionMementos.size(); ) {
                const MotionMemento& otherMemento = other.mMotionMementos.itemAt(j);
                if (memento.deviceId == otherMemento.deviceId
                        && memento.source == otherMemento.source) {
                    other.mMotionMementos.removeAt(j);
                } else {
                    j += 1;
                }
            }
            other.mMotionMementos.push(memento);
        }
    }
}

int32_t MirInputDispatcher::InputState::getFallbackKey(int32_t originalKeyCode) {
    ssize_t index = mFallbackKeys.indexOfKey(originalKeyCode);
    return index >= 0 ? mFallbackKeys.valueAt(index) : -1;
}

void MirInputDispatcher::InputState::setFallbackKey(int32_t originalKeyCode,
        int32_t fallbackKeyCode) {
    ssize_t index = mFallbackKeys.indexOfKey(originalKeyCode);
    if (index >= 0) {
        mFallbackKeys.replaceValueAt(index, fallbackKeyCode);
    } else {
        mFallbackKeys.add(originalKeyCode, fallbackKeyCode);
    }
}

void MirInputDispatcher::InputState::removeFallbackKey(int32_t originalKeyCode) {
    mFallbackKeys.removeItem(originalKeyCode);
}

bool MirInputDispatcher::InputState::shouldCancelKey(const KeyMemento& memento,
        const CancelationOptions& options) {
    if (options.keyCode != -1 && memento.keyCode != options.keyCode) {
        return false;
    }

    if (options.deviceId != -1 && memento.deviceId != options.deviceId) {
        return false;
    }

    switch (options.mode) {
    case CancelationOptions::CANCEL_ALL_EVENTS:
    case CancelationOptions::CANCEL_NON_POINTER_EVENTS:
        return true;
    case CancelationOptions::CANCEL_FALLBACK_EVENTS:
        return memento.flags & AKEY_EVENT_FLAG_FALLBACK;
    default:
        return false;
    }
}

bool MirInputDispatcher::InputState::shouldCancelMotion(const MotionMemento& memento,
        const CancelationOptions& options) {
    if (options.deviceId != -1 && memento.deviceId != options.deviceId) {
        return false;
    }

    switch (options.mode) {
    case CancelationOptions::CANCEL_ALL_EVENTS:
        return true;
    case CancelationOptions::CANCEL_POINTER_EVENTS:
        return memento.source & AINPUT_SOURCE_CLASS_POINTER;
    case CancelationOptions::CANCEL_NON_POINTER_EVENTS:
        return !(memento.source & AINPUT_SOURCE_CLASS_POINTER);
    default:
        return false;
    }
}


// --- MirInputDispatcher::Connection ---

MirInputDispatcher::Connection::Connection(const sp<InputChannel>& inputChannel, bool monitor) :
        status(STATUS_NORMAL), inputChannel(inputChannel), monitor(monitor),
        inputPublisher(inputChannel), inputPublisherBlocked(false) {
}

MirInputDispatcher::Connection::~Connection() {
}

const char* MirInputDispatcher::Connection::getWindowName() const {
    if (monitor) {
        return "monitor";
    }
    return "?";
}

const char* MirInputDispatcher::Connection::getStatusLabel() const {
    switch (status) {
    case STATUS_NORMAL:
        return "NORMAL";

    case STATUS_BROKEN:
        return "BROKEN";

    case STATUS_ZOMBIE:
        return "ZOMBIE";

    default:
        return "UNKNOWN";
    }
}

MirInputDispatcher::DispatchEntry* MirInputDispatcher::Connection::findWaitQueueEntry(uint32_t seq) {
    for (DispatchEntry* entry = waitQueue.head; entry != NULL; entry = entry->next) {
        if (entry->seq == seq) {
            return entry;
        }
    }
    return NULL;
}

// --- MirInputDispatcher::CommandEntry ---

MirInputDispatcher::CommandEntry::CommandEntry(Command command) :
    command(command), eventTime(0), keyEntry(NULL), userActivityEventType(0),
    seq(0), handled(false) {
}

MirInputDispatcher::CommandEntry::~CommandEntry() {
}

// --- MirInputDispatcherThread ---

MirInputDispatcherThread::MirInputDispatcherThread(const sp<MirInputDispatcherInterface>& dispatcher) :
        Thread(/*canCallJava*/ true), mDispatcher(dispatcher) {
}

MirInputDispatcherThread::~MirInputDispatcherThread() {
}

bool MirInputDispatcherThread::threadLoop() {
    mDispatcher->dispatchOnce();
    return true;
}

} // namespace android
