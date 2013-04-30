/****************************************************************************
**
** Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies).
** Contact: http://www.qt-project.org/
**
** This file is part of the examples of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:BSD$
** You may use this file under the terms of the BSD license as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of Nokia Corporation and its Subsidiary(-ies) nor
**     the names of its contributors may be used to endorse or promote
**     products derived from this software without specific prior written
**     permission.
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

import QtQuick 2.0

TemplateBase{
    scoreTarget: 2000
    timeTarget: 60
    moveTarget: 20
    mustClear: false
    goalText: "10 of 10<br><br>Score 2000 in one minute with less than 20 moves!"
    startingGrid: [ 3 , 2 , 3 , 1 , 3 , 3 , 4 , 1 , 3 , 3 ,
                    2 , 3 , 2 , 1 , 1 , 2 , 2 , 2 , 4 , 1 ,
                    2 , 4 , 4 , 4 , 3 , 1 , 4 , 4 , 4 , 1 ,
                    3 , 1 , 3 , 4 , 4 , 2 , 2 , 2 , 2 , 3 ,
                    2 , 1 , 4 , 4 , 3 , 3 , 1 , 1 , 3 , 2 ,
                    3 , 2 , 1 , 4 , 3 , 4 , 1 , 3 , 4 , 2 ,
                    3 , 3 , 1 , 4 , 4 , 4 , 2 , 1 , 2 , 3 ,
                    2 , 3 , 4 , 3 , 4 , 1 , 1 , 3 , 2 , 4 ,
                    4 , 4 , 1 , 2 , 4 , 3 , 2 , 2 , 2 , 4 ,
                    1 , 4 , 2 , 2 , 1 , 1 , 2 , 1 , 1 , 4 ,
                    1 , 4 , 3 , 3 , 3 , 1 , 3 , 4 , 4 , 2 ,
                    3 , 4 , 1 , 1 , 2 , 2 , 2 , 3 , 2 , 1 ,
                    3 , 3 , 4 , 3 , 1 , 1 , 1 , 4 , 4 , 3 ]
}
