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

#ifndef MIRQPA_INPUT_DISPATCHER_H
#define MIRQPA_INPUT_DISPATCHER_H

#include <androidfw/Input.h>
#include <androidfw/InputTransport.h>
#include <androidfw/IntSet.h>
#include <std/KeyedVector.h>
#include <std/Vector.h>
#include <std/Timers.h>
#include <std/RefBase.h>
#include <std/String8.h>
#include <std/Looper.h>
#include <std/atomic.h>
#include <std/Condition.h>
#include <std/Thread.h>
#include <std/Mutex.h>

#include <stddef.h>
#include <unistd.h>
#include <limits.h>

#include <InputListener.h>
#include <InputApplication.h>

#include <memory>

#include <QDebug>

namespace mir
{
namespace input
{
class InputReport;
}
}

namespace android {


/*
 * An input target specifies how an input event is to be dispatched to a particular window
 * including the window's input channel, control flags, a timeout, and an X / Y offset to
 * be added to input event coordinates to compensate for the absolute position of the
 * window area.
 */
struct InputTarget {
    enum {
        /* This flag indicates that the event is being delivered to a foreground application. */
        FLAG_FOREGROUND = 1 << 0,

        /* This flag indicates that the target of a MotionEvent is partly or wholly
         * obscured by another visible window above it.  The motion event should be
         * delivered with flag AMOTION_EVENT_FLAG_WINDOW_IS_OBSCURED. */
        FLAG_WINDOW_IS_OBSCURED = 1 << 1,

        /* This flag indicates that the pointer coordinates dispatched to the application
         * will be zeroed out to avoid revealing information to an application. This is
         * used in conjunction with FLAG_DISPATCH_AS_OUTSIDE to prevent apps not sharing
         * the same UID from watching all touches. */
        FLAG_ZERO_COORDS = 1 << 3,

        /* This flag indicates that the event should be sent as is.
         * Should always be set unless the event is to be transmuted. */
        FLAG_DISPATCH_AS_IS = 1 << 8,

        /* This flag indicates that a MotionEvent with AMOTION_EVENT_ACTION_DOWN falls outside
         * of the area of this target and so should instead be delivered as an
         * AMOTION_EVENT_ACTION_OUTSIDE to this target. */
        FLAG_DISPATCH_AS_OUTSIDE = 1 << 9,

        /* This flag indicates that a hover sequence is starting in the given window.
         * The event is transmuted into ACTION_HOVER_ENTER. */
        FLAG_DISPATCH_AS_HOVER_ENTER = 1 << 10,

        /* This flag indicates that a hover event happened outside of a window which handled
         * previous hover events, signifying the end of the current hover sequence for that
         * window.
         * The event is transmuted into ACTION_HOVER_ENTER. */
        FLAG_DISPATCH_AS_HOVER_EXIT = 1 << 11,

        /* This flag indicates that the event should be canceled.
         * It is used to transmute ACTION_MOVE into ACTION_CANCEL when a touch slips
         * outside of a window. */
        FLAG_DISPATCH_AS_SLIPPERY_EXIT = 1 << 12,

        /* This flag indicates that the event should be dispatched as an initial down.
         * It is used to transmute ACTION_MOVE into ACTION_DOWN when a touch slips
         * into a new window. */
        FLAG_DISPATCH_AS_SLIPPERY_ENTER = 1 << 13,

        /* Mask for all dispatch modes. */
        FLAG_DISPATCH_MASK = FLAG_DISPATCH_AS_IS
                | FLAG_DISPATCH_AS_OUTSIDE
                | FLAG_DISPATCH_AS_HOVER_ENTER
                | FLAG_DISPATCH_AS_HOVER_EXIT
                | FLAG_DISPATCH_AS_SLIPPERY_EXIT
                | FLAG_DISPATCH_AS_SLIPPERY_ENTER
    };

    // The input channel to be targeted.
    sp<InputChannel> inputChannel;

    // Flags for the input target.
    int32_t flags;

    // The x and y offset to add to a MotionEvent as it is delivered.
    // (ignored for KeyEvents)
    float xOffset, yOffset;

    // Scaling factor to apply to MotionEvent as it is delivered.
    // (ignored for KeyEvents)
    float scaleFactor;
};

/*
 * Input dispatcher policy interface.
 *
 * The input reader policy is used by the input reader to interact with the Window Manager
 * and other system components.
 *
 * The actual implementation is partially supported by callbacks into the DVM
 * via JNI.  This interface is also mocked in the unit tests.
 */
#if 0
class MirInputDispatcherPolicyInterface : public virtual RefBase {
protected:
    MirInputDispatcherPolicyInterface() { }
    virtual ~MirInputDispatcherPolicyInterface() { }

public:
    /* Notifies the system that an application is not responding.
     * Returns a new timeout to continue waiting, or 0 to abort dispatch. */
    virtual nsecs_t notifyANR(const sp<InputApplicationHandle>& inputApplicationHandle,
            const sp<MirInputWindowHandle>& inputWindowHandle) = 0;

    /* Notifies the system that an input channel is unrecoverably broken. */
    virtual void notifyInputChannelBroken(const sp<MirInputWindowHandle>& inputWindowHandle) = 0;
};
#endif


/* Notifies the system about input events generated by the input reader.
 * The dispatcher is expected to be mostly asynchronous. */
class MirInputDispatcherInterface : public virtual RefBase {
protected:
    MirInputDispatcherInterface() {
        if (gInstance != nullptr) {
            qFatal("MirInputDispatcher must be a singleton");
        }
        gInstance = this;
    }
    virtual ~MirInputDispatcherInterface() {
        Q_ASSERT(gInstance != nullptr);
        gInstance = nullptr;
    }

public:
    static MirInputDispatcherInterface* instance() {
        return gInstance;
    }

    virtual void notifyMotion(const NotifyMotionArgs* args, const InputTarget &target) = 0;

    /* Dumps the state of the input dispatcher.
     *
     * This method may be called on any thread (usually by the input manager). */
    virtual void dump(String8& dump) = 0;

    /* Called by the heatbeat to ensures that the dispatcher has not deadlocked. */
    virtual void monitor() = 0;

    /* Runs a single iteration of the dispatch loop.
     * Nominally processes one queued event, a timeout, or a response from an input consumer.
     *
     * This method should only be called on the input dispatcher thread.
     */
    virtual void dispatchOnce() = 0;

    /* Sets the input dispatching mode.
     *
     * This method may be called on any thread (usually by the input manager).
     */
    virtual void setInputDispatchMode(bool enabled, bool frozen) = 0;

    /* Registers or unregister input channels that may be used as targets for input events.
     * If monitor is true, the channel will receive a copy of all input events.
     *
     * These methods may be called on any thread (usually by the input manager).
     */
    virtual status_t registerInputChannel(const sp<InputChannel>& inputChannel,
            bool monitor) = 0;
    virtual status_t unregisterInputChannel(const sp<InputChannel>& inputChannel) = 0;

private:
    static MirInputDispatcherInterface *gInstance;
};

/* Dispatches events to input targets.  Some functions of the input dispatcher, such as
 * identifying input targets, are controlled by a separate policy object.
 *
 * IMPORTANT INVARIANT:
 *     Because the policy can potentially block or cause re-entrance into the input dispatcher,
 *     the input dispatcher never calls into the policy while holding its internal locks.
 *     The implementation is also carefully designed to recover from scenarios such as an
 *     input channel becoming unregistered while identifying input targets or processing timeouts.
 *
 *     Methods marked 'Locked' must be called with the lock acquired.
 *
 *     Methods marked 'LockedInterruptible' must be called with the lock acquired but
 *     may during the course of their execution release the lock, call into the policy, and
 *     then reacquire the lock.  The caller is responsible for recovering gracefully.
 *
 *     A 'LockedInterruptible' method may called a 'Locked' method, but NOT vice-versa.
 */
class MirInputDispatcher : public MirInputDispatcherInterface {
protected:
    virtual ~MirInputDispatcher();

public:
    explicit MirInputDispatcher(std::shared_ptr<mir::input::InputReport> const& input_report);

    virtual void dump(String8& dump);
    virtual void monitor();

    virtual void dispatchOnce();

    virtual void notifyKey(const NotifyKeyArgs* args);
    void notifyMotion(const NotifyMotionArgs* args, const InputTarget &target) override;
    virtual void notifyDeviceReset(const NotifyDeviceResetArgs* args);

    virtual void setInputDispatchMode(bool enabled, bool frozen);

    virtual status_t registerInputChannel(const sp<InputChannel>& inputChannel,
            bool monitor);
    virtual status_t unregisterInputChannel(const sp<InputChannel>& inputChannel);

private:
    std::shared_ptr<mir::input::InputReport> const input_report;

    template <typename T>
    struct Link {
        T* next;
        T* prev;

    protected:
        inline Link() : next(NULL), prev(NULL) { }
    };

    struct EventEntry : Link<EventEntry> {
        enum {
            TYPE_DEVICE_RESET,
            TYPE_KEY,
            TYPE_MOTION
        };

        mutable int32_t refCount;
        int32_t type;
        nsecs_t eventTime;
        uint32_t policyFlags;

        bool dispatchInProgress; // initially false, set to true while dispatching

        InputTarget target;

        void release();

        virtual void appendDescription(String8& msg) const = 0;

    protected:
        EventEntry(int32_t type, nsecs_t eventTime, uint32_t policyFlags);
        virtual ~EventEntry();
    };

    struct DeviceResetEntry : EventEntry {
        int32_t deviceId;

        DeviceResetEntry(nsecs_t eventTime, int32_t deviceId);
        virtual void appendDescription(String8& msg) const;

    protected:
        virtual ~DeviceResetEntry();
    };

    struct KeyEntry : EventEntry {
        int32_t deviceId;
        uint32_t source;
        int32_t action;
        int32_t flags;
        int32_t keyCode;
        int32_t scanCode;
        int32_t metaState;
        nsecs_t downTime;

        KeyEntry(nsecs_t eventTime,
                int32_t deviceId, uint32_t source, uint32_t policyFlags, int32_t action,
                int32_t flags, int32_t keyCode, int32_t scanCode, int32_t metaState,
                nsecs_t downTime);
        virtual void appendDescription(String8& msg) const;
        void recycle();

    protected:
        virtual ~KeyEntry();
    };

    struct MotionEntry : EventEntry {
        nsecs_t eventTime;
        int32_t deviceId;
        uint32_t source;
        int32_t action;
        int32_t flags;
        int32_t metaState;
        int32_t buttonState;
        int32_t edgeFlags;
        float xPrecision;
        float yPrecision;
        nsecs_t downTime;
        uint32_t pointerCount;
        PointerProperties pointerProperties[MAX_POINTERS];
        PointerCoords pointerCoords[MAX_POINTERS];

        MotionEntry(nsecs_t eventTime,
                int32_t deviceId, uint32_t source, uint32_t policyFlags, int32_t action,
                int32_t flags, int32_t metaState, int32_t buttonState, int32_t edgeFlags,
                float xPrecision, float yPrecision,
                nsecs_t downTime, uint32_t pointerCount,
                const PointerProperties* pointerProperties, const PointerCoords* pointerCoords);
        virtual void appendDescription(String8& msg) const;

    protected:
        virtual ~MotionEntry();
    };

    // Tracks the progress of dispatching a particular event to a particular connection.
    struct DispatchEntry : Link<DispatchEntry> {
        const uint32_t seq; // unique sequence number, never 0

        EventEntry* eventEntry; // the event to dispatch
        int32_t targetFlags;
        float xOffset;
        float yOffset;
        float scaleFactor;
        nsecs_t deliveryTime; // time when the event was actually delivered

        // Set to the resolved action and flags when the event is enqueued.
        int32_t resolvedAction;
        int32_t resolvedFlags;

        DispatchEntry(EventEntry* eventEntry,
                int32_t targetFlags, float xOffset, float yOffset, float scaleFactor);
        ~DispatchEntry();

        inline bool hasForegroundTarget() const {
            return targetFlags & InputTarget::FLAG_FOREGROUND;
        }

    private:
        static android_atomic_int32_t sNextSeqAtomic;

        static uint32_t nextSeq();
    };

    // A command entry captures state and behavior for an action to be performed in the
    // dispatch loop after the initial processing has taken place.  It is essentially
    // a kind of continuation used to postpone sensitive policy interactions to a point
    // in the dispatch loop where it is safe to release the lock (generally after finishing
    // the critical parts of the dispatch cycle).
    //
    // The special thing about commands is that they can voluntarily release and reacquire
    // the dispatcher lock at will.  Initially when the command starts running, the
    // dispatcher lock is held.  However, if the command needs to call into the policy to
    // do some work, it can release the lock, do the work, then reacquire the lock again
    // before returning.
    //
    // This mechanism is a bit clunky but it helps to preserve the invariant that the dispatch
    // never calls into the policy while holding its lock.
    //
    // Commands are implicitly 'LockedInterruptible'.
    struct CommandEntry;
    typedef void (MirInputDispatcher::*Command)(CommandEntry* commandEntry);

    class Connection;
    struct CommandEntry : Link<CommandEntry> {
        CommandEntry(Command command);
        ~CommandEntry();

        Command command;

        // parameters for the command (usage varies by command)
        sp<Connection> connection;
        nsecs_t eventTime;
        KeyEntry* keyEntry;
        int32_t userActivityEventType;
        uint32_t seq;
        bool handled;
    };

    // Generic queue implementation.
    template <typename T>
    struct Queue {
        T* head;
        T* tail;

        inline Queue() : head(NULL), tail(NULL) {
        }

        inline bool isEmpty() const {
            return !head;
        }

        inline void enqueueAtTail(T* entry) {
            entry->prev = tail;
            if (tail) {
                tail->next = entry;
            } else {
                head = entry;
            }
            entry->next = NULL;
            tail = entry;
        }

        inline void enqueueAtHead(T* entry) {
            entry->next = head;
            if (head) {
                head->prev = entry;
            } else {
                tail = entry;
            }
            entry->prev = NULL;
            head = entry;
        }

        inline void dequeue(T* entry) {
            if (entry->prev) {
                entry->prev->next = entry->next;
            } else {
                head = entry->next;
            }
            if (entry->next) {
                entry->next->prev = entry->prev;
            } else {
                tail = entry->prev;
            }
        }

        inline T* dequeueAtHead() {
            T* entry = head;
            head = entry->next;
            if (head) {
                head->prev = NULL;
            } else {
                tail = NULL;
            }
            return entry;
        }

        uint32_t count() const;
    };

    /* Specifies which events are to be canceled and why. */
    struct CancelationOptions {
        enum Mode {
            CANCEL_ALL_EVENTS = 0,
            CANCEL_POINTER_EVENTS = 1,
            CANCEL_NON_POINTER_EVENTS = 2,
            CANCEL_FALLBACK_EVENTS = 3
        };

        // The criterion to use to determine which events should be canceled.
        Mode mode;

        // Descriptive reason for the cancelation.
        const char* reason;

        // The specific keycode of the key event to cancel, or -1 to cancel any key event.
        int32_t keyCode;

        // The specific device id of events to cancel, or -1 to cancel events from any device.
        int32_t deviceId;

        CancelationOptions(Mode mode, const char* reason) :
                mode(mode), reason(reason), keyCode(-1), deviceId(-1) { }
    };

    /* Tracks dispatched key and motion event state so that cancelation events can be
     * synthesized when events are dropped. */
    class InputState {
    public:
        InputState();
        ~InputState();

        // Returns true if there is no state to be canceled.
        bool isNeutral() const;

        // Returns true if the specified source is known to have received a hover enter
        // motion event.
        bool isHovering(int32_t deviceId, uint32_t source) const;

        // Records tracking information for a key event that has just been published.
        // Returns true if the event should be delivered, false if it is inconsistent
        // and should be skipped.
        bool trackKey(const KeyEntry* entry, int32_t action, int32_t flags);

        // Records tracking information for a motion event that has just been published.
        // Returns true if the event should be delivered, false if it is inconsistent
        // and should be skipped.
        bool trackMotion(const MotionEntry* entry, int32_t action, int32_t flags);

        // Synthesizes cancelation events for the current state and resets the tracked state.
        void synthesizeCancelationEvents(nsecs_t currentTime,
                Vector<EventEntry*>& outEvents, const CancelationOptions& options);

        // Clears the current state.
        void clear();

        // Copies pointer-related parts of the input state to another instance.
        void copyPointerStateTo(InputState& other) const;

        // Gets the fallback key associated with a keycode.
        // Returns -1 if none.
        // Returns AKEYCODE_UNKNOWN if we are only dispatching the unhandled key to the policy.
        int32_t getFallbackKey(int32_t originalKeyCode);

        // Sets the fallback key for a particular keycode.
        void setFallbackKey(int32_t originalKeyCode, int32_t fallbackKeyCode);

        // Removes the fallback key for a particular keycode.
        void removeFallbackKey(int32_t originalKeyCode);

        inline const KeyedVector<int32_t, int32_t>& getFallbackKeys() const {
            return mFallbackKeys;
        }

    private:
        struct KeyMemento {
            int32_t deviceId;
            uint32_t source;
            int32_t keyCode;
            int32_t scanCode;
            int32_t metaState;
            int32_t flags;
            nsecs_t downTime;
            uint32_t policyFlags;
        };

        struct MotionMemento {
            int32_t deviceId;
            uint32_t source;
            int32_t flags;
            float xPrecision;
            float yPrecision;
            nsecs_t downTime;
            uint32_t pointerCount;
            PointerProperties pointerProperties[MAX_POINTERS];
            PointerCoords pointerCoords[MAX_POINTERS];
            bool hovering;
            uint32_t policyFlags;

            void setPointers(const MotionEntry* entry);
        };

        Vector<KeyMemento> mKeyMementos;
        Vector<MotionMemento> mMotionMementos;
        KeyedVector<int32_t, int32_t> mFallbackKeys;

        ssize_t findKeyMemento(const KeyEntry* entry) const;
        ssize_t findMotionMemento(const MotionEntry* entry, bool hovering) const;

        void addKeyMemento(const KeyEntry* entry, int32_t flags);
        void addMotionMemento(const MotionEntry* entry, int32_t flags, bool hovering);

        static bool shouldCancelKey(const KeyMemento& memento,
                const CancelationOptions& options);
        static bool shouldCancelMotion(const MotionMemento& memento,
                const CancelationOptions& options);
    };

    /* Manages the dispatch state associated with a single input channel. */
    class Connection : public RefBase {
    protected:
        virtual ~Connection();

    public:
        enum ConnStatus {
            // Everything is peachy.
            STATUS_NORMAL,
            // An unrecoverable communication error has occurred.
            STATUS_BROKEN,
            // The input channel has been unregistered.
            STATUS_ZOMBIE
        };

        ConnStatus status;
        sp<InputChannel> inputChannel; // never null
        bool monitor;
        InputPublisher inputPublisher;
        InputState inputState;

        // True if the socket is full and no further events can be published until
        // the application consumes some of the input.
        bool inputPublisherBlocked;

        // Queue of events that need to be published to the connection.
        Queue<DispatchEntry> outboundQueue;

        // Queue of events that have been published to the connection but that have not
        // yet received a "finished" response from the application.
        Queue<DispatchEntry> waitQueue;

        explicit Connection(const sp<InputChannel>& inputChannel, bool monitor);

        inline const char* getInputChannelName() const { return c_str(inputChannel->getName()); }

        const char* getWindowName() const;
        const char* getStatusLabel() const;

        DispatchEntry* findWaitQueueEntry(uint32_t seq);
    };

    enum DropReason {
        DROP_REASON_NOT_DROPPED = 0,
        DROP_REASON_POLICY = 1,
        DROP_REASON_APP_SWITCH = 2,
        DROP_REASON_DISABLED = 3,
        DROP_REASON_BLOCKED = 4,
        DROP_REASON_STALE = 5
    };

    Mutex mLock;

    Condition mDispatcherIsAliveCondition;

    sp<Looper> mLooper;

    EventEntry* mPendingEvent;
    Queue<EventEntry> mInboundQueue;
    Queue<CommandEntry> mCommandQueue;

    void dispatchOnceInnerLocked(nsecs_t* nextWakeupTime);

    // Enqueues an inbound event.  Returns true if mLooper->wake() should be called.
    bool enqueueInboundEventLocked(EventEntry* entry);

    // Cleans up input state when dropping an inbound event.
    void dropInboundEventLocked(EventEntry* entry, DropReason dropReason);

    // App switch latency optimization.
    bool mAppSwitchSawKeyDown;
    nsecs_t mAppSwitchDueTime;

    static bool isAppSwitchKeyCode(int32_t keyCode);
    bool isAppSwitchKeyEventLocked(KeyEntry* keyEntry);
    bool isAppSwitchPendingLocked();
    void resetPendingAppSwitchLocked(bool handled);

    // Stale event latency optimization.
    static bool isStaleEventLocked(nsecs_t currentTime, EventEntry* entry);

    // Blocked event latency optimization.  Drops old events when the user intends
    // to transfer focus to a new application.
    EventEntry* mNextUnblockedEvent;

    // All registered connections mapped by channel file descriptor.
    KeyedVector<int, sp<Connection> > mConnectionsByFd;

    ssize_t getConnectionIndexLocked(const sp<InputChannel>& inputChannel);

    // Input channels that will receive a copy of all input events.
    Vector<sp<InputChannel> > mMonitoringChannels;

    // Deferred command processing.
    bool runCommandsLockedInterruptible();
    CommandEntry* postCommandLocked(Command command);

    // Inbound event processing.
    void drainInboundQueueLocked();
    void releasePendingEventLocked();
    void releaseInboundEventLocked(EventEntry* entry);

    // Dispatch state.
    bool mDispatchEnabled;
    bool mDispatchFrozen;

    // Dispatcher state at time of last ANR.
    String8 mLastANRState;

    // Dispatch inbound events.
    bool dispatchDeviceResetLocked(DeviceResetEntry* entry);
    bool dispatchKeyLocked(
            nsecs_t currentTime, KeyEntry* entry, DropReason* dropReason);
    bool dispatchMotionLocked(
            nsecs_t currentTime, MotionEntry* entry,
            DropReason* dropReason);
    void dispatchEventLocked(nsecs_t currentTime, EventEntry* entry,
            const Vector<InputTarget>& inputTargets);

    void logOutboundKeyDetailsLocked(const char* prefix, const KeyEntry* entry);
    void logOutboundMotionDetailsLocked(const char* prefix, const MotionEntry* entry);

    // Keeping track of ANR timeouts.
    enum InputTargetWaitCause {
        INPUT_TARGET_WAIT_CAUSE_NONE,
        INPUT_TARGET_WAIT_CAUSE_SYSTEM_NOT_READY,
        INPUT_TARGET_WAIT_CAUSE_APPLICATION_NOT_READY
    };

    InputTargetWaitCause mInputTargetWaitCause;
    nsecs_t mInputTargetWaitStartTime;
    nsecs_t mInputTargetWaitTimeoutTime;
    bool mInputTargetWaitTimeoutExpired;
    sp<InputApplicationHandle> mInputTargetWaitApplicationHandle;

    // Finding targets for input events.
    void resetANRTimeoutsLocked();

    void addMonitoringTargetsLocked(Vector<InputTarget>& inputTargets);

    // Manage the dispatch cycle for a single connection.
    // These methods are deliberately not Interruptible because doing all of the work
    // with the mutex held makes it easier to ensure that connection invariants are maintained.
    // If needed, the methods post commands to run later once the critical bits are done.
    void prepareDispatchCycleLocked(nsecs_t currentTime, const sp<Connection>& connection,
            EventEntry* eventEntry, const InputTarget* inputTarget);
    void enqueueDispatchEntriesLocked(nsecs_t currentTime, const sp<Connection>& connection,
            EventEntry* eventEntry, const InputTarget* inputTarget);
    void enqueueDispatchEntryLocked(const sp<Connection>& connection,
            EventEntry* eventEntry, const InputTarget* inputTarget, int32_t dispatchMode);
    void startDispatchCycleLocked(nsecs_t currentTime, const sp<Connection>& connection);
    void finishDispatchCycleLocked(nsecs_t currentTime, const sp<Connection>& connection,
            uint32_t seq, bool handled);
    void abortBrokenDispatchCycleLocked(const sp<Connection>& connection,
            bool notify);
    void drainDispatchQueueLocked(Queue<DispatchEntry>* queue);
    void releaseDispatchEntryLocked(DispatchEntry* dispatchEntry);
    static int handleReceiveCallback(int fd, int events, void* data);

    void synthesizeCancelationEventsForAllConnectionsLocked(
            const CancelationOptions& options);
    void synthesizeCancelationEventsForInputChannelLocked(const sp<InputChannel>& channel,
            const CancelationOptions& options);
    void synthesizeCancelationEventsForConnectionLocked(const sp<Connection>& connection,
            const CancelationOptions& options);

    // Reset and drop everything the dispatcher is doing.
    void resetAndDropEverythingLocked(const char* reason);

    // Dump state.
    void dumpDispatchStateLocked(String8& dump);
    void logDispatchStateLocked();

    // Registration.
    void removeMonitorChannelLocked(const sp<InputChannel>& inputChannel);
    status_t unregisterInputChannelLocked(const sp<InputChannel>& inputChannel, bool notify);

    // Add or remove a connection to the mActiveConnections vector.
    void activateConnectionLocked(Connection* connection);
    void deactivateConnectionLocked(Connection* connection);

    // Interesting events that we might like to log or tell the framework about.
    void onDispatchCycleFinishedLocked(
            nsecs_t currentTime, const sp<Connection>& connection, uint32_t seq, bool handled);
    //void onDispatchCycleBrokenLocked(const sp<Connection>& connection);

    // Outbound policy interactions.
    //void doNotifyInputChannelBrokenLockedInterruptible(CommandEntry* commandEntry);
    void doDispatchCycleFinishedLockedInterruptible(CommandEntry* commandEntry);
    bool afterKeyEventLockedInterruptible(const sp<Connection>& connection,
            DispatchEntry* dispatchEntry, KeyEntry* keyEntry, bool handled);
    void initializeKeyEvent(KeyEvent* event, const KeyEntry* entry);
};

/* Enqueues and dispatches input events, endlessly. */
class MirInputDispatcherThread : public Thread {
public:
    explicit MirInputDispatcherThread(const sp<MirInputDispatcherInterface>& dispatcher);
    ~MirInputDispatcherThread();

private:
    virtual bool threadLoop();

    sp<MirInputDispatcherInterface> mDispatcher;
};

} // namespace android

#endif // MIRQPA_INPUT_DISPATCHER_H
