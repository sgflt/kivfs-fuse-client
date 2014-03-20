USER_OBJS :=

LIBS := `pkg-config fuse sqlite3 libssl --libs` -lulockmgr -lpthread -lgssapi -lkivfscore

