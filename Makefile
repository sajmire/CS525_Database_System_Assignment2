assignment2:
	gcc -o assignment2 test_assign2_1.c buffer_mgr.c buffer_mgr_stat.c storage_mgr.c dberror.c -I.
	./assignment2
assign2_clock:
	 gcc -o assign2_clock test_clock2_2.c buffer_mgr.c storage_mgr.c buffer_mgr_stat.c dberror.c -I.
	./assign2_clock  
