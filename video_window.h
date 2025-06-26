#include "includes.h"

class VideoWindow : public Gtk::Window {
public:
    VideoWindow()
    {
        set_title("Video Playback");
        fullscreen(); // start fullscreen
        signal_key_press_event().connect(sigc::mem_fun(*this, &VideoWindow::on_key_press), false);
    }

    bool on_key_press(GdkEventKey* event)
    {
        if (event->keyval == GDK_KEY_f || event->keyval == GDK_KEY_F) {
            if (is_fullscreen) {
                unfullscreen();
            } else {
                fullscreen();
            }
            is_fullscreen = !is_fullscreen;
            return true;
        }
        return false;
    }

private:
    bool is_fullscreen = true;
};
