/************************************************************************
* file name         : round_progress_widget.h
* ----------------- :
* creation time     : 2018/05/17
* author            : Victor Zarubkin
* email             : v.s.zarubkin@gmail.com
* ----------------- :
* description       : The file contains declaration of RoundProgressWidget.
* ----------------- :
* license           : Lightweight profiler library for c++
*                   : Copyright(C) 2016-2019  Sergey Yagovtsev, Victor Zarubkin
*                   :
*                   : Licensed under either of
*                   :     * MIT license (LICENSE.MIT or http://opensource.org/licenses/MIT)
*                   :     * Apache License, Version 2.0, (LICENSE.APACHE or http://www.apache.org/licenses/LICENSE-2.0)
*                   : at your option.
*                   :
*                   : The MIT License
*                   :
*                   : Permission is hereby granted, free of charge, to any person obtaining a copy
*                   : of this software and associated documentation files (the "Software"), to deal
*                   : in the Software without restriction, including without limitation the rights
*                   : to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
*                   : of the Software, and to permit persons to whom the Software is furnished
*                   : to do so, subject to the following conditions:
*                   :
*                   : The above copyright notice and this permission notice shall be included in all
*                   : copies or substantial portions of the Software.
*                   :
*                   : THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
*                   : INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
*                   : PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
*                   : LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
*                   : TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
*                   : USE OR OTHER DEALINGS IN THE SOFTWARE.
*                   :
*                   : The Apache License, Version 2.0 (the "License")
*                   :
*                   : You may not use this file except in compliance with the License.
*                   : You may obtain a copy of the License at
*                   :
*                   : http://www.apache.org/licenses/LICENSE-2.0
*                   :
*                   : Unless required by applicable law or agreed to in writing, software
*                   : distributed under the License is distributed on an "AS IS" BASIS,
*                   : WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*                   : See the License for the specific language governing permissions and
*                   : limitations under the License.
************************************************************************/

#ifndef ROUND_PROGRESS_WIDGET_H
#define ROUND_PROGRESS_WIDGET_H

#include <stdint.h>
#include <QColor>
#include <QDialog>
#include <QWidget>

class RoundProgressIndicator : public QWidget
{
Q_OBJECT

    using Parent = QWidget;
    using This = RoundProgressIndicator;

    QString             m_text;
    QColor        m_background;
    QColor             m_color;
    int8_t             m_value;
    bool             m_pressed;
    bool m_cancelButtonEnabled;

public:

    Q_PROPERTY(QColor color READ color WRITE setColor);
    Q_PROPERTY(QColor background READ background WRITE setBackground);

    explicit RoundProgressIndicator(QWidget* parent = nullptr);
    ~RoundProgressIndicator() override;

    int value() const;
    void setValue(int value);
    void reset();

    QColor background() const;
    QColor color() const;

    bool cancelButtonEnabled() const;
    void setCancelButtonEnabled(bool enabled);

signals:

    void cancelButtonClicked();

public slots:

    void setBackground(QColor color);
    void setBackground(QString color);
    void setColor(QColor color);
    void setColor(QString color);

protected:

    void showEvent(QShowEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void enterEvent(QEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;

}; // end of class RoundProgressIndicator.

class RoundProgressWidget : public QWidget
{
    Q_OBJECT

    using Parent = QWidget;
    using This = RoundProgressWidget;

public:

    Q_PROPERTY(bool topTitlePosition READ isTopTitlePosition WRITE setTopTitlePosition NOTIFY titlePositionChanged);

    enum TitlePosition : int8_t
    {
        Top = 0,
        Bottom,
    };

private:

    class QLabel*                   m_title;
    QWidget*             m_indicatorWrapper;
    RoundProgressIndicator*     m_indicator;
    TitlePosition           m_titlePosition;

public:

    explicit RoundProgressWidget(QWidget* parent = nullptr);
    explicit RoundProgressWidget(const QString& title, QWidget* parent = nullptr);
    ~RoundProgressWidget() override;

    void setTitle(const QString& title);
    int value() const;
    TitlePosition titlePosition() const;
    bool isTopTitlePosition() const;

    bool cancelButtonEnabled() const;
    void setCancelButtonEnabled(bool enabled);

public slots:

    void setValue(int value);
    void reset();
    void setTitlePosition(TitlePosition pos);
    void setTopTitlePosition(bool isTop);

signals:

    void valueChanged(int value);
    void finished();
    void titlePositionChanged();
    void canceled();

}; // end of class RoundProgressWidget.

class RoundProgressDialog : public QDialog
{
    Q_OBJECT

    using Parent = QDialog;
    using This = RoundProgressDialog;

    RoundProgressWidget* m_progress;
    QColor             m_background;

public:

    Q_PROPERTY(QColor background READ background WRITE setBackground);

    explicit RoundProgressDialog(const QString& title, QWidget* parent = nullptr);
    ~RoundProgressDialog() override;

    QColor background() const;

    bool cancelButtonEnabled() const;
    void setCancelButtonEnabled(bool enabled);

protected:

    void showEvent(QShowEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

public slots:

    void setBackground(QColor color);
    void setBackground(QString color);
    void setValue(int value);

signals:

    void valueChanged(int value);
    void finished();
    void canceled();

}; // end of RoundProgressDialog.

#endif // ROUND_PROGRESS_WIDGET_H
