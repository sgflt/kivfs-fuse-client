USER_OBJS :=

LIBS := `pkg-config fuse sqlite3 libssl krb5 --libs` -lulockmgr -lpthread -lgssapi -lkivfscore

