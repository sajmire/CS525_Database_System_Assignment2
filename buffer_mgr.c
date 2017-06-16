#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include "storage_mgr.h"
#include "buffer_mgr.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>



typedef struct BM_TraverseList	 {
	
	struct BM_TraverseList	 * next;
	struct BM_TraverseList	 * prev;
	int poolIndex;
	int pageIndex; 
}BM_TraverseList	;


typedef struct BM_DataManage {
	int * FrameContents; 
	int * FixCounts;  
	int *ClockFlag;  
	int AvailablePool; 
	char * PagePool; 
	bool * DirtyFlags; 
	BM_TraverseList	 *HEAD;
	BM_TraverseList	 *CURRENT_HANDLE;
	BM_TraverseList	 *TAIL;
	
}BM_DataManage;


int  NumReadIO;  
int  NumWriteIO; 




RC initBufferPool(BM_BufferPool *const bm, const char *const pageFileName, 
		const int numPages, ReplacementStrategy strategy, 
		void *stratData)
{
	
	
	
	bm->strategy = strategy;
	bm->mgmtData = ((BM_DataManage *)malloc (sizeof(BM_DataManage)));
	bm->pageFile = (char *)pageFileName;
	bm->numPages = numPages;
	
	
	BM_DataManage *mgmtData = (BM_DataManage *)bm->mgmtData;

	mgmtData->AvailablePool = numPages;
	NumReadIO = 0;
	NumWriteIO = 0;
	mgmtData->DirtyFlags = (bool *)malloc (numPages*sizeof(bool));
	mgmtData->FixCounts = (int *)malloc(numPages*sizeof(int));
	mgmtData->FrameContents = (int *)malloc(numPages*sizeof(int));
	mgmtData->HEAD = NULL;
	mgmtData->CURRENT_HANDLE = NULL;
	mgmtData->TAIL = NULL;
	mgmtData->ClockFlag = (int *)malloc(numPages*PAGE_SIZE*sizeof(int));
	mgmtData->PagePool = (char *)malloc(numPages*PAGE_SIZE*sizeof(char));
	
	
	int i;
	for (i=0; i< numPages; i++){
		*(mgmtData->ClockFlag + i) = 0;
		*(mgmtData->FrameContents + i) = NO_PAGE;
		*(mgmtData->DirtyFlags + i) = false;
		*(mgmtData->FixCounts + i) = 0;
		
	}
	return RC_OK;
}

RC shutdownBufferPool(BM_BufferPool *const bm){
	BM_DataManage *mgmtData = (BM_DataManage *)bm->mgmtData;
	int i;
	for (i=0; i< bm->numPages; i++){
	  if (*(((BM_DataManage *)bm->mgmtData)->FixCounts+i)> 0) {
	  	return RC_FAIL_SHUTDOWN_POOL;
	  }
	 }
	forceFlushPool(bm);
	
	BM_DataManage * Data = bm->mgmtData;
	free(Data->ClockFlag);
	free(Data->FrameContents);
	NumReadIO = 0;
	free(Data->DirtyFlags);
	free(Data->FixCounts);
	free(Data->PagePool);
	Data->ClockFlag = NULL;
	Data->FrameContents = NULL;
	NumWriteIO = 0;
	Data->DirtyFlags = NULL;
	Data->FixCounts = NULL;
	
	NumReadIO = 0;
	NumWriteIO = 0;
	Data->PagePool = NULL;
	Data->HEAD = NULL;
	Data->CURRENT_HANDLE = NULL;
	Data->TAIL = NULL;
	free(bm->mgmtData);
	bm->mgmtData = NULL;
	return RC_OK;
}


RC markDirty (BM_BufferPool *const bm, BM_PageHandle *const page){
	
	int poolIndex,flag=0;
	BM_TraverseList	 *current = NULL;
	current = (((BM_DataManage *)bm->mgmtData)->HEAD);
	while (current!=NULL){
		if (current->pageIndex == page->pageNum){
			poolIndex = current->poolIndex;
			flag=1;
		}
		current = current->next;
	}
	if(flag==0)
	poolIndex= RC_PAGE_NOT_FOUND_IN_CACHE;
	*(((BM_DataManage *)bm->mgmtData)->DirtyFlags+poolIndex) = true;
	return RC_OK;
}

RC unpinPage (BM_BufferPool *const bm, BM_PageHandle *const page){
	
	
	BM_TraverseList	 *current = NULL;
	current = (((BM_DataManage *)bm->mgmtData)->HEAD);
	int poolIndex,flag=0;
	while (current!=NULL){
		if (current->pageIndex == page->pageNum){
			poolIndex = current->poolIndex;
			flag=1;
		}
		current = current->next;
	}
	if(flag==0)
	poolIndex= RC_PAGE_NOT_FOUND_IN_CACHE;
	(*(((BM_DataManage *)bm->mgmtData)->FixCounts+poolIndex))--;
	return RC_OK;
	

}


RC forcePage (BM_BufferPool *const bm, BM_PageHandle *const page){
	
	int poolIndex,flag=0;
	int pageNum = page->pageNum;
	
	
	BM_TraverseList	 *current = NULL;
	current = (((BM_DataManage *)bm->mgmtData)->HEAD);
	while (current!=NULL){
		if (current->pageIndex == page->pageNum){
			poolIndex = current->poolIndex;
			flag=1;
		}
		current = current->next;
	}
	if(flag==0)
	poolIndex= RC_PAGE_NOT_FOUND_IN_CACHE;
	if (*(((BM_DataManage *)bm->mgmtData)->FixCounts+poolIndex)> 0){
		return RC_FAIL_FORCE_PAGE_DUETO_PIN_EXIT;
	} else {
		SM_FileHandle fh;
		char * data = page->data;
		openPageFile (bm->pageFile, &fh);
		
		writeBlock(pageNum, &fh, data);
		*(((BM_DataManage *)bm->mgmtData)->DirtyFlags+poolIndex)= false; /* set dirty flag to false after flashing into disk */
		NumWriteIO++;
		closePageFile(&fh);	
		return RC_OK;
	}
}

RC forceFlushPool(BM_BufferPool *const bm){
	
	SM_FileHandle fh;
	int pageIndex;
	char * data;
	
	openPageFile (bm->pageFile, &fh);
	int i;
	for (i=0; i< bm->numPages; i++){
		if (*(((BM_DataManage *)bm->mgmtData)->DirtyFlags+i)== true) 
		{
			data = ((BM_DataManage *)bm->mgmtData)->PagePool + i*PAGE_SIZE*sizeof(char);
			pageIndex = *(((BM_DataManage *)bm->mgmtData)->FrameContents+i);
			writeBlock(pageIndex, &fh, data);
			*(((BM_DataManage *)bm->mgmtData)->DirtyFlags+i)= false; 
			NumWriteIO++;			
		}		
	}
	closePageFile(&fh);	
	return RC_OK;		
}


RC pinPage (BM_BufferPool *const bm, BM_PageHandle *const page, 
		const PageNumber pageNum){

	
	SM_FileHandle fh;
	openPageFile (bm->pageFile, &fh);
	
	if (fh.totalNumPages < (pageNum+1)){
		ensureCapacity(pageNum+1,&fh);
	}
	page->pageNum = pageNum;
	BM_TraverseList	 *temp = NULL;
	temp = (((BM_DataManage *)bm->mgmtData)->HEAD);
	int i=1;
	while (temp!=NULL)
	{
		if (temp->pageIndex == pageNum)
		{
			i=0;
			break;
		}
		temp = temp->next;
	}
	if (i==0)
	{	
		
		BM_TraverseList	 *current = NULL;
		current = (((BM_DataManage *)bm->mgmtData)->HEAD);
		int poolIndex,flag=0;
		while (current!=NULL)
		{
		if (current->pageIndex == pageNum)
		{
			poolIndex=current->poolIndex;
			flag=1;
		}
		current = current->next;
	}
	if(flag==0)
	poolIndex= RC_PAGE_NOT_FOUND_IN_CACHE;
		
		page->data = (((BM_DataManage *)bm->mgmtData)->PagePool)+poolIndex*PAGE_SIZE*sizeof(char);

		(*(((BM_DataManage *)bm->mgmtData)->FixCounts+poolIndex))++;
		if (bm->strategy == RS_LRU)
		{
			
			BM_TraverseList	 *current = NULL;
			BM_TraverseList	 *tail = NULL;
			current = (((BM_DataManage *)bm->mgmtData)->HEAD);
			tail = (((BM_DataManage *)bm->mgmtData)->TAIL);
			while (current != NULL)
			{
				if (current->pageIndex == pageNum)
				{
					break;
				}
			
				current = current->next;
			}
		
		if (current != tail)
		{
		(((BM_DataManage *)bm->mgmtData)->TAIL)->next = current;
		current->next->prev = current->prev;
		
			if(current == (((BM_DataManage *)bm->mgmtData)->HEAD))
			{
				(((BM_DataManage *)bm->mgmtData)->HEAD) = (((BM_DataManage *)bm->mgmtData)->HEAD)->next;
			} 
			else 
			{
			current->prev->next = current->next;
			}
		current->prev = (((BM_DataManage *)bm->mgmtData)->TAIL);				
		(((BM_DataManage *)bm->mgmtData)->TAIL) = (((BM_DataManage *)bm->mgmtData)->TAIL)->next;
		(((BM_DataManage *)bm->mgmtData)->TAIL)->next = NULL;	
		}
	}
		(*(((BM_DataManage *)bm->mgmtData)->ClockFlag+poolIndex)) = 1; 
		return RC_OK;
	} 
	
	else 
	{
		if (((BM_DataManage *)bm->mgmtData)->AvailablePool>0){
			
			BM_TraverseList	 *handle = ((BM_TraverseList	 *)malloc (sizeof(BM_TraverseList	)));
			
			int poolIndex = bm->numPages - ((BM_DataManage *)bm->mgmtData)->AvailablePool;
			handle->pageIndex = pageNum;
			handle->poolIndex = poolIndex;
			handle->next = NULL;
			
			if(((BM_DataManage *)bm->mgmtData)->HEAD == NULL)
			{
				(((BM_DataManage *)bm->mgmtData)->HEAD) = handle;
				(((BM_DataManage *)bm->mgmtData)->TAIL) = handle;
				handle->prev = NULL;
				handle->next = NULL;
			} 
			
			else
			{
				(((BM_DataManage *)bm->mgmtData)->TAIL)->next = handle;
				handle->prev = (((BM_DataManage *)bm->mgmtData)->TAIL);
				(((BM_DataManage *)bm->mgmtData)->TAIL) = handle;
			}
		
		page->data = (((BM_DataManage *)bm->mgmtData)->PagePool)+poolIndex*PAGE_SIZE*sizeof(char);
		*(((BM_DataManage *)bm->mgmtData)->FrameContents+poolIndex) = pageNum;
		((BM_DataManage *)bm->mgmtData)->AvailablePool--;
		(*(((BM_DataManage *)bm->mgmtData)->FixCounts+poolIndex))++;
		

		} 
		
		else 
		{	
		     int poolIndex;
			
			if (bm->strategy == RS_FIFO || bm->strategy == RS_LRU){
		BM_TraverseList	 * temp = (((BM_DataManage *)bm->mgmtData)->HEAD);

		while (temp != NULL) {
		    if(*(((BM_DataManage *)bm->mgmtData)->FixCounts+temp->poolIndex)>0)
			 {
			 temp = temp->next;
			 }
			 else
			 break;
		}
		if (temp == NULL) {
			poolIndex= RC_ALL_PAGE_RESOURCE_OCCUPIED;
		} else {
			if(temp != (((BM_DataManage *)bm->mgmtData)->TAIL)){
				(((BM_DataManage *)bm->mgmtData)->TAIL)->next = temp;

				temp->next->prev = temp->prev;
				if(temp == (((BM_DataManage *)bm->mgmtData)->HEAD)){
					(((BM_DataManage *)bm->mgmtData)->HEAD) = (((BM_DataManage *)bm->mgmtData)->HEAD)->next;
				} else {
					temp->prev->next = temp->next;
				}
				temp->prev = (((BM_DataManage *)bm->mgmtData)->TAIL);				
				(((BM_DataManage *)bm->mgmtData)->TAIL) = (((BM_DataManage *)bm->mgmtData)->TAIL)->next;
				(((BM_DataManage *)bm->mgmtData)->TAIL)->next = NULL;
			}

			if(*(((BM_DataManage *)bm->mgmtData)->DirtyFlags+(((BM_DataManage *)bm->mgmtData)->TAIL)->poolIndex)== true){
				char * memory = ((BM_DataManage *)bm->mgmtData)->PagePool + (((BM_DataManage *)bm->mgmtData)->TAIL)->poolIndex*PAGE_SIZE*sizeof(char);
				int old_pageNum = (((BM_DataManage *)bm->mgmtData)->TAIL)->pageIndex;
				writeBlock(old_pageNum, &fh, memory);
				*(((BM_DataManage *)bm->mgmtData)->DirtyFlags+(((BM_DataManage *)bm->mgmtData)->TAIL)->poolIndex)= false;
				NumWriteIO++;
			}
			(((BM_DataManage *)bm->mgmtData)->TAIL)->pageIndex = pageNum;
			*(((BM_DataManage *)bm->mgmtData)->FrameContents+(((BM_DataManage *)bm->mgmtData)->TAIL)->poolIndex) = pageNum;
			poolIndex= (((BM_DataManage *)bm->mgmtData)->TAIL)->poolIndex;
		}
	} 
	else if (bm->strategy == RS_CLOCK){
		BM_TraverseList	 * temp = NULL;
		if ((((BM_DataManage *)bm->mgmtData)->CURRENT_HANDLE) == NULL){
			(((BM_DataManage *)bm->mgmtData)->CURRENT_HANDLE) = (((BM_DataManage *)bm->mgmtData)->HEAD);	
		}
		while (*(((BM_DataManage *)bm->mgmtData)->ClockFlag+(((BM_DataManage *)bm->mgmtData)->CURRENT_HANDLE)->poolIndex)== 1){
			*(((BM_DataManage *)bm->mgmtData)->ClockFlag+(((BM_DataManage *)bm->mgmtData)->CURRENT_HANDLE)->poolIndex) = 0; 
			if ((((BM_DataManage *)bm->mgmtData)->CURRENT_HANDLE) == (((BM_DataManage *)bm->mgmtData)->TAIL)){
				(((BM_DataManage *)bm->mgmtData)->CURRENT_HANDLE) = (((BM_DataManage *)bm->mgmtData)->HEAD); 
			} else {
				(((BM_DataManage *)bm->mgmtData)->CURRENT_HANDLE) = (((BM_DataManage *)bm->mgmtData)->CURRENT_HANDLE)->next;
			}	
		}
		(((BM_DataManage *)bm->mgmtData)->CURRENT_HANDLE)->pageIndex = pageNum; 
		*(((BM_DataManage *)bm->mgmtData)->FrameContents+(((BM_DataManage *)bm->mgmtData)->CURRENT_HANDLE)->poolIndex) = pageNum;	
		*(((BM_DataManage *)bm->mgmtData)->ClockFlag+(((BM_DataManage *)bm->mgmtData)->CURRENT_HANDLE)->poolIndex) = 1;
		temp = (((BM_DataManage *)bm->mgmtData)->CURRENT_HANDLE);
		
		if ((((BM_DataManage *)bm->mgmtData)->CURRENT_HANDLE) == (((BM_DataManage *)bm->mgmtData)->TAIL)){
				(((BM_DataManage *)bm->mgmtData)->CURRENT_HANDLE) = (((BM_DataManage *)bm->mgmtData)->HEAD); 
			} else {
				(((BM_DataManage *)bm->mgmtData)->CURRENT_HANDLE) = (((BM_DataManage *)bm->mgmtData)->CURRENT_HANDLE)->next;
			}	
		poolIndex= temp->poolIndex;
	}
	
	else
	poolIndex= -1;
			(*(((BM_DataManage *)bm->mgmtData)->FixCounts+poolIndex))++;
			page->data = (((BM_DataManage *)bm->mgmtData)->PagePool)+poolIndex*PAGE_SIZE*sizeof(char);

		}
		
		readBlock(page->pageNum, &fh, page->data);
		NumReadIO++;
	}
	closePageFile(&fh);		
	return RC_OK;
}






PageNumber *getFrameContents (BM_BufferPool *const bm)
{
	return ((BM_DataManage *)bm->mgmtData)->FrameContents;
}


bool *getDirtyFlags (BM_BufferPool *const bm)
{
	return ((BM_DataManage *)bm->mgmtData)->DirtyFlags;
}


int *getFixCounts (BM_BufferPool *const bm)
{
	return ((BM_DataManage *)bm->mgmtData)->FixCounts;
}

int getNumReadIO (BM_BufferPool *const bm)
{
	return NumReadIO;
}

int getNumWriteIO (BM_BufferPool *const bm)
{
	return NumWriteIO;
}
