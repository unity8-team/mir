/*
 * Copyright (C) 2013 Canonical, Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

import QtQuick 2.0

Item {
    id: showable

    property bool available: true
    property bool shown: true

    /* If your showable supports on demand content creation/destruction,
       set this to false when destroyed and true when ready to be shown.
       NOTE: You should load your content when "required" is true and
       destroy when "required" is false
    */
    property bool created: true
    property bool required
    property bool __shouldShow: false

    property list<QtObject> hides
    property var showAnimation
    property var hideAnimation

    // automatically set the target on showAnimation and hideAnimation to be the
    // showable itself
    onShowAnimationChanged: if (showAnimation) showAnimation["target"] = showable
    onHideAnimationChanged: if (hideAnimation) hideAnimation["target"] = showable

    Component.onCompleted: required = shown;

    function __hideOthers() {
        var i
        for (i=0; i<hides.length; i++) {
            hides[i].hide()
        }
    }

    function show() {
        required = true;
        if (created) {
            __reallyShow(true);
        } else {
            __shouldShow = true;
        }
    }

    onCreatedChanged: {
        if (created && __shouldShow) {
            __reallyShow(true);
            __shouldShow = false;
        }
    }

    function showWithoutAnimation() {
        __reallyShow(false);
    }

    function __reallyShow(animated) {
        if (!available) {
            return false;
        }

        __hideOthers();

        if (hideAnimation != undefined && hideAnimation.running) {
            hideAnimation.stop();
        }

        if (showAnimation != undefined) {
            if (animated) {
                if (!showAnimation.running) {
                    showAnimation.restart()
                }
            } else {
                showAnimation.start();
                showAnimation.complete();
            }
        } else {
            visible = true;
        }

        shown = true;
        return true;
    }

    function hide() {
        if (showAnimation != undefined && showAnimation.running) {
            showAnimation.stop()
        }
        if (hideAnimation != undefined) {
            if (!hideAnimation.running) {
                hideAnimation.restart()
            }
        } else {
            visible = false
            required = false
        }

        shown = false
    }

    Connections {
        target: hideAnimation ? hideAnimation: null
        onRunningChanged: {
            if (!hideAnimation.running) {
                required = false;
            }
        }
    }
}
