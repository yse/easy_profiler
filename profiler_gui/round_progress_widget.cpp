/************************************************************************
* file name         : round_progress_widget.cpp
* ----------------- :
* creation time     : 2018/05/17
* author            : Victor Zarubkin
* email             : v.s.zarubkin@gmail.com
* ----------------- :
* description       : The file contains implementation of RoundProgressWidget.
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

#include <math.h>

#include <QFontMetrics>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QLabel>
#include <QPainter>
#include <QStyle>
#include <QVariant>
#include <QVBoxLayout>

#include <easy/utility.h>

#include "common_functions.h"
#include "globals.h"
#include "round_progress_widget.h"

#ifdef max
# undef max
#endif

RoundProgressIndicator::RoundProgressIndicator(QWidget* parent)
    : Parent(parent)
    , m_text("0%")
    , m_background(Qt::transparent)
    , m_color(Qt::green)
    , m_value(0)
    , m_pressed(false)
    , m_cancelButtonEnabled(false)
{
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAutoFillBackground(false);
    setProperty("hover", false);
}

RoundProgressIndicator::~RoundProgressIndicator()
{

}

bool RoundProgressIndicator::cancelButtonEnabled() const
{
    return m_cancelButtonEnabled;
}

void RoundProgressIndicator::setCancelButtonEnabled(bool enabled)
{
    m_cancelButtonEnabled = enabled;
    update();
}

int RoundProgressIndicator::value() const
{
    return m_value;
}

void RoundProgressIndicator::setValue(int value)
{
    m_value = static_cast<int8_t>(estd::clamp(0, value, 100));
    m_text = QString("%1%").arg(m_value);
    update();
}

void RoundProgressIndicator::reset()
{
    m_value = 0;
    m_text = "0%";
    update();
}

QColor RoundProgressIndicator::background() const
{
    return m_background;
}

void RoundProgressIndicator::setBackground(QColor color)
{
    m_background = std::move(color);
    update();
}

void RoundProgressIndicator::setBackground(QString color)
{
    m_background.setNamedColor(color);
    update();
}

QColor RoundProgressIndicator::color() const
{
    return m_color;
}

void RoundProgressIndicator::setColor(QColor color)
{
    m_color = std::move(color);
    update();
}

void RoundProgressIndicator::setColor(QString color)
{
    m_color.setNamedColor(color);
    update();
}

void RoundProgressIndicator::showEvent(QShowEvent* event)
{
    Parent::showEvent(event);

    const QFontMetrics fm(font());
    const QString text = QStringLiteral("100%");
    const int size = std::max(fm.width(text), fm.height()) + px(4 * 4);

    setFixedSize(size, size);
}

void RoundProgressIndicator::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(Qt::NoBrush);

    const auto px4 = px(4);
    auto r = rect().adjusted(px4, px4, -px4, -px4);
    auto p = painter.pen();

    // Draw circle
    p.setWidth(px4);
    p.setColor(m_background);
    painter.setPen(p);
    painter.drawArc(r, 0, 360 * 16);

    p.setColor(m_color);
    painter.setPen(p);
    painter.drawArc(r, 90 * 16, -1 * static_cast<int>(m_value) * 16 * 360 / 100);

    const bool hover = property("hover").toBool();

    if (hover && m_cancelButtonEnabled)
    {
        // Draw cancel button (red cross)

        const auto hquarter = px4 + (r.width() >> 2);
        const auto vquarter = px4 + (r.height() >> 2);
        r.adjust(hquarter, vquarter, -hquarter, -vquarter);

        p.setWidth(px(2));
        p.setColor(QColor::fromRgb(m_pressed ? profiler::colors::Red900 : profiler::colors::Red500));
        p.setCapStyle(Qt::SquareCap);

        painter.setPen(p);
        painter.setBrush(Qt::NoBrush);
        painter.drawLine(r.topLeft(), r.bottomRight());
        painter.drawLine(r.bottomLeft(), r.topRight());
    }
    else
    {
        // Draw text
        p.setWidth(px(1));
        p.setColor(palette().foreground().color());
        painter.setPen(p);
        painter.drawText(r, Qt::AlignCenter, m_text);
    }
}

void RoundProgressIndicator::enterEvent(QEvent* event) {
    Parent::enterEvent(event);
    profiler_gui::updateProperty(this, "hover", true);
}

void RoundProgressIndicator::leaveEvent(QEvent* event) {
    Parent::leaveEvent(event);
    profiler_gui::updateProperty(this, "hover", false);
}

void RoundProgressIndicator::mousePressEvent(QMouseEvent* event)
{
    Parent::mousePressEvent(event);
    m_pressed = true;
    update();
}

void RoundProgressIndicator::mouseReleaseEvent(QMouseEvent* event)
{
    Parent::mouseReleaseEvent(event);

    const bool hover = property("hover").toBool();
    const bool pressed = m_pressed;

    m_pressed = false;
    update();

    if (pressed && hover && m_cancelButtonEnabled)
    {
        emit cancelButtonClicked();
    }
}

void RoundProgressIndicator::mouseMoveEvent(QMouseEvent* event)
{
    if (m_pressed)
    {
        const bool hover = property("hover").toBool();
        if (rect().contains(event->pos()))
        {
            if (!hover)
            {
                profiler_gui::updateProperty(this, "hover", true);
            }
        }
        else if (hover)
        {
            profiler_gui::updateProperty(this, "hover", false);
        }
    }

    Parent::mouseMoveEvent(event);
}

RoundProgressWidget::RoundProgressWidget(QWidget* parent)
    : RoundProgressWidget(QString(), parent)
{
}

RoundProgressWidget::RoundProgressWidget(const QString& title, QWidget* parent)
    : Parent(parent)
    , m_title(new QLabel(title, this))
    , m_indicatorWrapper(new QWidget(this))
    , m_indicator(new RoundProgressIndicator(this))
    , m_titlePosition(RoundProgressWidget::Top)
{
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAutoFillBackground(false);

    auto wlay = new QHBoxLayout(m_indicatorWrapper);
    wlay->setContentsMargins(0, 0, 0, 0);
    wlay->addWidget(m_indicator, 0, Qt::AlignCenter);

    auto lay = new QVBoxLayout(this);
    lay->addWidget(m_title);
    lay->addWidget(m_indicatorWrapper);

    connect(m_indicator, &RoundProgressIndicator::cancelButtonClicked, this, &RoundProgressWidget::canceled);
}

RoundProgressWidget::~RoundProgressWidget()
{

}

void RoundProgressWidget::setTitle(const QString& title)
{
    m_title->setText(title);
}

int RoundProgressWidget::value() const
{
    return m_indicator->value();
}

void RoundProgressWidget::setValue(int value)
{
    const auto prev = m_indicator->value();
    const auto newValue = static_cast<int8_t>(estd::clamp(0, value, 100));
    if (prev == newValue)
        return;

    m_indicator->setValue(newValue);
    const auto v = m_indicator->value();

    emit valueChanged(v);

    if (v == 100)
        emit finished();
}

void RoundProgressWidget::reset()
{
    m_indicator->reset();
}

RoundProgressWidget::TitlePosition RoundProgressWidget::titlePosition() const
{
    return m_titlePosition;
}

void RoundProgressWidget::setTitlePosition(TitlePosition pos)
{
    const auto prev = m_titlePosition;
    if (prev == pos)
        return;

    m_titlePosition = pos;

    auto lay = static_cast<QVBoxLayout*>(layout());
    if (pos == RoundProgressWidget::Top)
    {
        lay->removeWidget(m_indicatorWrapper);
        lay->removeWidget(m_title);

        lay->addWidget(m_title);
        lay->addWidget(m_indicatorWrapper);
    }
    else
    {
        lay->removeWidget(m_title);
        lay->removeWidget(m_indicator);

        lay->addWidget(m_indicator);
        lay->addWidget(m_title);
    }

    emit titlePositionChanged();

    update();
}

bool RoundProgressWidget::isTopTitlePosition() const
{
    return m_titlePosition == RoundProgressWidget::Top;
}

void RoundProgressWidget::setTopTitlePosition(bool isTop)
{
    setTitlePosition(isTop ? RoundProgressWidget::Top : RoundProgressWidget::Bottom);
}

bool RoundProgressWidget::cancelButtonEnabled() const
{
    return m_indicator->cancelButtonEnabled();
}

void RoundProgressWidget::setCancelButtonEnabled(bool enabled)
{
    m_indicator->setCancelButtonEnabled(enabled);
}

RoundProgressDialog::RoundProgressDialog(const QString& title, QWidget* parent)
    : Parent(parent)
    , m_progress(new RoundProgressWidget(title, this))
    , m_background(Qt::transparent)
{
    setWindowTitle(profiler_gui::DEFAULT_WINDOW_TITLE);
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAutoFillBackground(false);

    auto lay = new QVBoxLayout(this);
    lay->addWidget(m_progress);

    connect(m_progress, &RoundProgressWidget::valueChanged, this, &RoundProgressDialog::valueChanged);
    connect(m_progress, &RoundProgressWidget::finished, this, &RoundProgressDialog::finished);
    connect(m_progress, &RoundProgressWidget::canceled, this, &RoundProgressDialog::canceled);
}

RoundProgressDialog::~RoundProgressDialog()
{

}

QColor RoundProgressDialog::background() const
{
    return m_background;
}

void RoundProgressDialog::setBackground(QColor color)
{
    m_background = std::move(color);
    update();
}

void RoundProgressDialog::setBackground(QString color)
{
    m_background.setNamedColor(color);
    update();
}

bool RoundProgressDialog::cancelButtonEnabled() const
{
    return m_progress->cancelButtonEnabled();
}

void RoundProgressDialog::setCancelButtonEnabled(bool enabled)
{
    m_progress->setCancelButtonEnabled(enabled);
}

void RoundProgressDialog::showEvent(QShowEvent* event)
{
    Parent::showEvent(event);
    adjustSize();
}

void RoundProgressDialog::setValue(int value)
{
    m_progress->setValue(value);
    if (value == 100)
        hide();
}

void RoundProgressDialog::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(m_background);
    painter.drawRect(0, 0, width(), height());
}
