# CS525_Database_System_Assignment2
********************************************************/

                   Team Information:

/********************************************************/

Group No 12
Members:
1) Tanmaya Bhatt (A20381345)
2) Shushupti Ajmire (A20385389)
3) Vikram Gavisiddeshwara (A20356257)
4) Saisruthi Kolanupaka (A20388341)

/********************************************************/

                   Buffer Manager: 

/********************************************************/

Buffer manager manages a buffer of blocks in memory including reading/flushing to disk and block replacement.

We have used replacement strategies FIFO and LRU for the buffer manager.


/********************************************************/

                      How to Run:

/********************************************************/

make assignment2  (to run default testcases)

make assign2_clock (to run test cases, to test the clock replacement strategy)

/*****************************************************************************/

  Storage manager from assignement 1 has been used for managing the page files.

/******************************************************************************/

/********************************************************/

               Files and their purpose:

/********************************************************/

1. Buffer_mgr.c: 
This file contains the main logic of the Buffer manager.

2.BUffer_mgr.h:
It provides the  buffer manager interface.

3. Storage_mgr.c:
This file contains the main logic of the storage manager used from assignement 1.

4. storage_mgr.h:
It provides the  storage manager interface.

5. dberror.h, dberror.c : 
They provide the error handling strategies for the storage manager application.

6. test_assign2_1.c: 
It provides test cases to test the buffer manager application.

7.test_clock2_2.c:
It provides test cases to test the clock replacement strategy for buffer manager application.

/********************************************************/

               Functions used and their purpose:

/********************************************************/

1.INITBUFFERPOOL : Creates the buffer manager and initializes the metadata structures.

2.SHUTDOWNBUFFERPOOL: Used to free up all resources associated with the buffer pool.

3.PIN PAGE: Pin page is used to map the page from disk to a frame in main memory. If buffer pool is full then replacement strategies (FIFO or LRU)

are used to replace the page.  

4.UNPIN PAGE: Used to unpin the page using page number.

5.FORCEFLUSHPOOL: Used for writting all the dirty pages from the buffer pool to the disk.

6.BM_TraverseList : Structure used for traversal of pages from disk to main memory.

7.BM_DataManage: Structure used to manage buffer contents.
