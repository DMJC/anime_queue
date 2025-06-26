#include "includes.h"
#include "video_window.h"

class FillerColumns : public Gtk::TreeModel::ColumnRecord {
public:
    FillerColumns() {
        add(file_path);
		add(duration_seconds);
    }

    Gtk::TreeModelColumn<std::string> file_path;
	Gtk::TreeModelColumn<int> duration_seconds;
};

class PlaylistColumns : public Gtk::TreeModel::ColumnRecord {
public:
    PlaylistColumns() {
        add(start_time);
        add(start_time_seconds);
        add(file_path);
		add(duration_seconds);
    }

    Gtk::TreeModelColumn<std::string> start_time;
    Gtk::TreeModelColumn<int> start_time_seconds; // internal use
    Gtk::TreeModelColumn<std::string> file_path;
	Gtk::TreeModelColumn<int> duration_seconds;
};

enum class PlayingType { None, Playlist, Filler };
PlayingType currently_playing = PlayingType::None;
int current_end_time = 0;

class MainWindow : public Gtk::Window {
public:
    MainWindow()
        : playlist_label("Playlist"), filler_label("Filler Content"),
          add_playlist_button("Add Playlist Item"), add_filler_button("Add Filler Item"),
          bulk_add_playlist_button("Add Playlist Items from CSV"),
          start_button("Start") {

        set_title("Video Scheduler");
        set_default_size(800, 600);

        add(main_vbox);

        main_hbox.set_spacing(10);
        main_vbox.pack_start(main_hbox);

        // Playlist side
        playlist_box.pack_start(playlist_label, Gtk::PACK_SHRINK);
        playlist_view.set_model(playlist_store = Gtk::ListStore::create(playlist_columns));

        auto cell_time = Gtk::make_managed<Gtk::CellRendererText>();
        cell_time->property_editable() = true;
        cell_time->signal_edited().connect(sigc::mem_fun(*this, &MainWindow::on_start_time_edited));

        int col_num = playlist_view.append_column("Start Time", *cell_time);
        playlist_view.get_column(col_num - 1)->add_attribute(cell_time->property_text(), playlist_columns.start_time);

        playlist_view.append_column("File", playlist_columns.file_path);
        playlist_box.pack_start(playlist_view);
        playlist_box.pack_start(add_playlist_button, Gtk::PACK_SHRINK);
        playlist_box.pack_start(bulk_add_playlist_button, Gtk::PACK_SHRINK);
        add_playlist_button.signal_clicked().connect(sigc::mem_fun(*this, &MainWindow::on_add_playlist_item));
        bulk_add_playlist_button.signal_clicked().connect(sigc::mem_fun(*this, &MainWindow::on_bulk_add_playlist_item));
        // Filler side
        filler_box.pack_start(filler_label, Gtk::PACK_SHRINK);
        filler_view.set_model(filler_store = Gtk::ListStore::create(filler_columns));
        filler_view.append_column("File", filler_columns.file_path);
        filler_box.pack_start(filler_view);
        filler_box.pack_start(add_filler_button, Gtk::PACK_SHRINK);
        add_filler_button.signal_clicked().connect(sigc::mem_fun(*this, &MainWindow::on_add_filler_item));

        main_hbox.pack_start(playlist_box);
        main_hbox.pack_start(filler_box);
        main_vbox.pack_start(start_button, Gtk::PACK_SHRINK);

        start_button.signal_clicked().connect(sigc::mem_fun(*this, &MainWindow::on_start_clicked));

        show_all_children();
    }

private:
    Gtk::Box main_vbox{Gtk::ORIENTATION_VERTICAL};
    Gtk::Box main_hbox{Gtk::ORIENTATION_HORIZONTAL};
    Gtk::Box playlist_box{Gtk::ORIENTATION_VERTICAL};
    Gtk::Box filler_box{Gtk::ORIENTATION_VERTICAL};

    Gtk::Label playlist_label, filler_label;
    Gtk::TreeView playlist_view, filler_view;
    Gtk::Button add_playlist_button, add_filler_button, start_button, bulk_add_playlist_button;

    PlaylistColumns playlist_columns;
    FillerColumns filler_columns;

    Glib::RefPtr<Gtk::ListStore> playlist_store, filler_store;

    GstElement* current_player = nullptr;
    bool running = false;

    int get_video_duration(const std::string& filepath) {
        GstElement* pipeline = gst_element_factory_make("playbin", nullptr);
        g_object_set(pipeline, "uri", Glib::filename_to_uri(filepath).c_str(), nullptr);

        gst_element_set_state(pipeline, GST_STATE_PAUSED);
        GstStateChangeReturn ret = gst_element_get_state(pipeline, nullptr, nullptr, GST_CLOCK_TIME_NONE);
    
        gint64 duration = 0;
        if (ret == GST_STATE_CHANGE_SUCCESS && gst_element_query_duration(pipeline, GST_FORMAT_TIME, &duration)) {
            gst_element_set_state(pipeline, GST_STATE_NULL);
            gst_object_unref(pipeline);
            return static_cast<int>(duration / GST_SECOND); // seconds
        }

        gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_object_unref(pipeline);
        return 60; // fallback
    }

    void on_add_playlist_item() {
        Gtk::FileChooserDialog dialog("Choose a media file", Gtk::FILE_CHOOSER_ACTION_OPEN);
        dialog.add_button("Cancel", Gtk::RESPONSE_CANCEL);
        dialog.add_button("Open", Gtk::RESPONSE_OK);

        if (dialog.run() == Gtk::RESPONSE_OK) {
	        std::string filepath = dialog.get_filename();
	        int duration = get_video_duration(filepath);
            Gtk::TreeModel::Row row = *(playlist_store->append());
            row[playlist_columns.start_time] = "09:00AM";
            row[playlist_columns.start_time_seconds] = 9 * 3600;
            row[playlist_columns.file_path] = dialog.get_filename();
	        row[playlist_columns.duration_seconds] = duration;
			std::cout << "Filename: " << filepath << std::endl;
			std::cout << "Duration: " << duration << std::endl;
        }
    }

	void on_bulk_add_playlist_item() {
	    Gtk::FileChooserDialog dialog("Choose CSV file", Gtk::FILE_CHOOSER_ACTION_OPEN);
	    dialog.add_button("Cancel", Gtk::RESPONSE_CANCEL);
	    dialog.add_button("Open", Gtk::RESPONSE_OK);
	
	    auto filter = Gtk::FileFilter::create();
	    filter->set_name("CSV files");
	    filter->add_pattern("*.csv");
	    dialog.add_filter(filter);
	
	    if (dialog.run() != Gtk::RESPONSE_OK)
	        return;
	
	    std::string filepath = dialog.get_filename();
	    std::ifstream file(filepath);
	    if (!file.is_open()) {
	        std::cerr << "Failed to open CSV file: " << filepath << std::endl;
	        return;
	    }

	    std::string line;
	    while (std::getline(file, line)) {
	        std::istringstream ss(line);
	        std::string start_time, filename;
	
	        if (!std::getline(ss, start_time, ',') || !std::getline(ss, filename))
	            continue;
	
	        std::tm tm{};
	        std::istringstream ts(start_time);
	        ts >> std::get_time(&tm, "%I:%M%p");  // e.g., "09:30AM"
	
	        if (ts.fail()) {
	            std::cerr << "Invalid time format: " << start_time << std::endl;
	            continue;
	        }
	
	        int seconds = tm.tm_hour * 3600 + tm.tm_min * 60;
	        std::string filepath = filename;
	        int duration = get_video_duration(filepath);	
	        Gtk::TreeModel::Row row = *(playlist_store->append());
	        row[playlist_columns.start_time] = start_time;
	        row[playlist_columns.start_time_seconds] = seconds;
	        row[playlist_columns.file_path] = filepath;
	        row[playlist_columns.duration_seconds] = duration;
			std::cout << "Filename: " << filepath << std::endl;
			std::cout << "Start Time: " << seconds << std::endl;
			std::cout << "Duration: " << duration << std::endl;
	    }
	}

    void on_add_filler_item() {
        Gtk::FileChooserDialog dialog("Choose a filler file", Gtk::FILE_CHOOSER_ACTION_OPEN);
	    dialog.set_select_multiple(true); // Enable multiple selection
        dialog.add_button("Cancel", Gtk::RESPONSE_CANCEL);
        dialog.add_button("Open", Gtk::RESPONSE_OK);

        if (dialog.run() == Gtk::RESPONSE_OK) {
        	std::vector<std::string> filenames = dialog.get_filenames();
        	for (const auto& filepath : filenames) {
        	    Gtk::TreeModel::Row row = *(filler_store->append());
        	    row[filler_columns.file_path] = filepath;
	            int duration = get_video_duration(filepath);
	            row[filler_columns.duration_seconds] = duration;
        	}
        }
    }

    void on_start_time_edited(const Glib::ustring& path, const Glib::ustring& new_text) {
        std::tm tm{};
        std::istringstream ss(new_text);
        ss >> std::get_time(&tm, "%I:%M%p");
        if (!ss.fail()) {
            Gtk::TreeModel::iterator iter = playlist_store->get_iter(path);
            if (iter) {
                Gtk::TreeModel::Row row = *iter;
                row[playlist_columns.start_time] = new_text;
                row[playlist_columns.start_time_seconds] = tm.tm_hour * 3600 + tm.tm_min * 60;
            }
        } else {
            std::cerr << "Invalid time format: " << new_text << std::endl;
        }
    }

    void on_start_clicked() {
        running = true;
        Glib::signal_timeout().connect_seconds(sigc::mem_fun(*this, &MainWindow::check_schedule), 1);
    }

    bool check_schedule() {
        if (!running)
            return false;

        auto now = std::time(nullptr);
        std::tm* local_tm = std::localtime(&now);
        int current_seconds = local_tm->tm_hour * 3600 + local_tm->tm_min * 60 + local_tm->tm_sec;

        // Check if we should start a playlist item
        for (auto& row : playlist_store->children()) {
            int start = row[playlist_columns.start_time_seconds];
		    int duration = row[playlist_columns.duration_seconds];
            if (start == current_seconds) {
                play_file(row[playlist_columns.file_path]);
                currently_playing = PlayingType::Playlist;
                current_end_time = current_seconds + duration;
                return true;
            }
        }

        // If something is playing and not finished, do nothing
        if (currently_playing != PlayingType::None && current_seconds < current_end_time)
            return true;

        // Playlist finished, check if next playlist item is in the future
        int next_playlist_time = 24 * 3600; // end of day
        for (auto& row : playlist_store->children()) {
            int start = row[playlist_columns.start_time_seconds];
            if (start > current_seconds && start < next_playlist_time)
                next_playlist_time = start;
        }

        // Time until next playlist item
        int gap = next_playlist_time - current_seconds;

        if (gap > 10) { // If we have enough time for a filler
            for (auto& row : filler_store->children()) {
		   		int duration = row[filler_columns.duration_seconds];
				std::cout << "Duration: " << row[filler_columns.duration_seconds] << std::endl;
                play_file(row[filler_columns.file_path]);
                currently_playing = PlayingType::Filler;
                // Assume duration = 30s for simplicity
                current_end_time = std::min(current_seconds + duration, next_playlist_time);
                break;
            }
        /*} else { ()
		//SHOW AVCON LOGO HERE
		*/
		}
        return true;
    }

	VideoWindow* video_window = nullptr;

	void play_file(const std::string& path)
	{
	    if (current_player) {
	        gst_element_set_state(current_player, GST_STATE_NULL);
	        gst_object_unref(current_player);
	        current_player = nullptr;	
	    }
		
	    // Create the playbin
	    current_player = gst_element_factory_make("playbin", nullptr);
	    if (!current_player) {
	        std::cerr << "Failed to create playbin.\n";
	        return;
	    }
	
	    // Create the autovideosink
	    GstElement* sink = gst_element_factory_make("autovideosink", nullptr);
	    if (!sink) {
	        std::cerr << "Failed to create autovideosink.\n";
	        return;
	    }
	
	    // Set the sink on the player
	    g_object_set(current_player, "video-sink", sink, NULL);
	
	    // Set the URI to the file
	    std::string uri = Glib::filename_to_uri(path);
	    g_object_set(current_player, "uri", uri.c_str(), NULL);
	
	    // Play
	    gst_element_set_state(current_player, GST_STATE_PLAYING);
	}
};

int main(int argc, char* argv[]) {
    gst_init(&argc, &argv);
    auto app = Gtk::Application::create(argc, argv, "org.gtkmm.scheduler");
    MainWindow window;
    return app->run(window);
}
