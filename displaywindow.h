#ifndef DISPLAYWINDOW_H
#define DISPLAYWINDOW_H

#include <qpa/qplatformwindow.h>

#include <mirplatform/mir/graphics/display_buffer.h>

// DisplayWindow wraps the whatever implementation Mir creates of a DisplayBuffer,
// which is the buffer output for an individual display.

class DisplayWindow : public QPlatformWindow
{
public:
    explicit DisplayWindow(QWindow *window, mir::graphics::DisplayBuffer*);

    QRect geometry() const override;
    void setGeometry(const QRect &rect) override;
    WId winId() const { return m_winId; }

    void swapBuffers();
    void makeCurrent();
    void doneCurrent();

private:
    WId m_winId;
    mir::graphics::DisplayBuffer *m_displayBuffer;
};

#endif // DISPLAYWINDOW_H
