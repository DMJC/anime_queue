build:
	g++ con_schedule.cpp -o con_sched `pkg-config --cflags --libs gtkmm-3.0 gstreamer-1.0 gstreamer-video-1.0` -std=c++14
clean:
	rm con_sched;
