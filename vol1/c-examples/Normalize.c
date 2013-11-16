#include "aifh-vol1.h"

typedef struct _PROCESS_PAIR {
	NORM_DATA *norm;
	DATA_SET *data;
} _PROCESS_PAIR;

static void _AppendUniqueClass(NORM_DATA_ITEM *item, char *className) {
	NORM_DATA_CLASS *lastClass;
	NORM_DATA_CLASS *newClass;
	int done;

	/* First scan the list and perform two functions */
	/* Function 1: Does this class already exist? */
	/* Function 2: Find the last item so that we can append later if the item does not exist */
	lastClass = item->firstClass;
	done=0;
	while(!done && lastClass!=NULL) {
		/* exit if we already have one */
		if(!strcmp(lastClass->name,className)) {
			return;
		}
		if( lastClass->next == NULL ) {
			done = 1;
		} else {
			lastClass=lastClass->next;
		}
	}

	/* Create a new class item */
	newClass = (NORM_DATA_CLASS*)calloc(1,sizeof(NORM_DATA_CLASS));
	newClass->name = strdup(className);

	/* Now add it */
	if( lastClass==NULL ) {
		/* New first item */
		item->firstClass = newClass;
	} else {
		lastClass->next = newClass;
	}

	/* Increase the class count */
	item->classCount++;
}

static void _AnalyzeCallbackColumn (void *s, size_t len, void *data) 
{
	NORM_DATA *norm;
	NORM_DATA_ITEM *col;
	int colNum;

	norm = (NORM_DATA *)data;

	/* Find the column definition */
	col = norm->firstItem;
	colNum = norm->_currentColumn;
	norm->_currentColumn++;

	while(colNum>0 && col!=NULL) {
		
		col = col->next;
		colNum--;
	}

	if( col==NULL ) {
		printf("The file had too many columns\n");
		exit(1);
	}

	/* Read the row */
	if(norm->rowCount==0 ) {
		/* Read header */
		col->name = strdup((char*)s);
		
	
	} else {
		/* Read regular data */
		double d = atof((char*)s);
		if( norm->rowCount==1 ) {
			col->actualHigh=d;
			col->actualLow=d;
		} else {
			switch(col->type) {
			case NORM_TYPE_RANGE:
				col->actualHigh=MAX(col->actualHigh,d);
				col->actualLow=MIN(col->actualLow,d);
				break;
			case NORM_CLASS_EQUILATERAL:
			case NORM_CLASS_ONEOFN:
				_AppendUniqueClass(col,(char*)s);
				break;
			}
		}
	}
}

static void _AnalyzeCallbackRow (int c, void *data) 
{
	NORM_DATA *norm;

	norm = (NORM_DATA *)data;
	norm->_currentColumn = 0;
	norm->rowCount++;
}

static void _ProcessCallbackColumn (void *s, size_t len, void *data) 
{
	_PROCESS_PAIR *pair;
	NORM_DATA *norm;
	NORM_DATA_ITEM *col;
	int colNum;

	pair = (_PROCESS_PAIR*)data;
	norm = pair->norm;

	/* Find the column definition */
	col = norm->firstItem;
	colNum = norm->_currentColumn;
	norm->_currentColumn++;

	while(colNum>0 && col!=NULL) {
		
		col = col->next;
		colNum--;
	}

	if( col==NULL ) {
		printf("The file had too many columns\n");
		exit(1);
	}

	/* normalize the column */
	switch(col->type) {
		case NORM_TYPE_RANGE:
			break;
		case NORM_CLASS_ONEOFN:
			break;
	}
}

static void _ProcessCallbackRow (int c, void *data) 
{
	_PROCESS_PAIR *pair;
	
	pair = (_PROCESS_PAIR*)data;

	pair->norm->_currentColumn = 0;
}

NORM_DATA *NormCreate() {
	NORM_DATA *result = (NORM_DATA *)calloc(1,sizeof(NORM_DATA));
	return result;
}

void NormDelete(NORM_DATA *norm) {
	NORM_DATA_ITEM *item, *t;
	NORM_DATA_CLASS *cl,*tcl;

	item = norm->firstItem;
	while(item!=NULL) {
		t = (NORM_DATA_ITEM *)item->next;

		/* free any classes that this item might have */
		cl = item->firstClass;
		while(cl!=NULL) {
			tcl = (NORM_DATA_CLASS*)cl->next;
			free(cl);
			cl = tcl;
		}

		free(item->name);
		free(item);
		item = t;
	}

	free(norm);

}

void NormDefRange(NORM_DATA *norm, double low, double high) {
	NORM_DATA_ITEM *last = norm->firstItem;
	NORM_DATA_ITEM *newItem;

	/* Find the last item */ 

	while( last!=NULL && last->next != NULL ) {
		last = (NORM_DATA_ITEM *)last->next;
	}

	/* Create the new item */
	newItem = (NORM_DATA_ITEM*)calloc(1,sizeof(NORM_DATA_ITEM));
	newItem->type = NORM_TYPE_RANGE;
	newItem->targetHigh = high;
	newItem->targetLow = low;
	newItem->next = NULL;

	/* Link the new item */
	if( last==NULL ) {
		/* Are we adding the first item */
		norm->firstItem = newItem;
	} else {
		/* Link to the end of the chain */
		last->next = (NORM_DATA_ITEM *)newItem;
	}
}

void NormDefClass(NORM_DATA *norm, int type, double low, double high) {
	NORM_DATA_ITEM *last = norm->firstItem;
	NORM_DATA_ITEM *newItem;

	/* Find the last item */ 

	while( last!=NULL && last->next != NULL ) {
		last = (NORM_DATA_ITEM *)last->next;
	}

	/* Create the new item */
	newItem = (NORM_DATA_ITEM*)calloc(1,sizeof(NORM_DATA_ITEM));
	newItem->type = type;
	newItem->targetHigh = high;
	newItem->targetLow = low;
	newItem->next = NULL;

	/* Link the new item */
	if( last==NULL ) {
		/* Are we adding the first item */
		norm->firstItem = newItem;
	} else {
		/* Link to the end of the chain */
		last->next = (NORM_DATA_ITEM *)newItem;
	}
}

void NormAnalyze(NORM_DATA *norm, char *filename) {
	FILE *fp;
	struct csv_parser p;
	char buf[1024];
	size_t bytes_read;

	// first, analyze the file
	if (csv_init(&p, CSV_APPEND_NULL) != 0) exit(EXIT_FAILURE);
	fp = fopen(filename, "rb");
	if (!fp)
	{ 
		printf("Could not open: %s\n", filename);
		exit(EXIT_FAILURE); 
	}

	norm->_currentColumn = 0;

	/* Read the file */
	while ((bytes_read=fread(buf, 1, 1024, fp)) > 0)
		if (csv_parse(&p, buf, bytes_read, _AnalyzeCallbackColumn, _AnalyzeCallbackRow, norm) != bytes_read) {
			fprintf(stderr, "Error while parsing file: %s\n",
			csv_strerror(csv_error(&p)) );
			exit(EXIT_FAILURE);
		}

	/* Handle any final data.  May call the callbacks once more */
	csv_fini(&p, _AnalyzeCallbackColumn, _AnalyzeCallbackRow, norm);

	/* Cleanup */
	fclose(fp);
	csv_free(&p);

	/* Decrease row count by one due to header */
	norm->rowCount--;
}

int CalculateActualCount(NORM_DATA *norm,int start, int size) {
	NORM_DATA_ITEM *item;
	int result;
	int columnIndex;

	item = norm->firstItem;

	result = 0;
	columnIndex = 0;

	while(item!=NULL) {
		if(columnIndex>=start && columnIndex<(start+size)) {
			switch(item->type) {
			case NORM_TYPE_RANGE:
				result+=1;
				break;
			case NORM_CLASS_EQUILATERAL:
				result+=item->classCount-1;
				break;
			case NORM_CLASS_ONEOFN:
				result+=item->classCount;
				break;
			}
		}

		item=item->next;
		columnIndex++;
	}

	return result;
}

DATA_SET *NormProcess(NORM_DATA *norm, char *filename, int inputCount, int outputCount) {
	FILE *fp;
	struct csv_parser p;
	char buf[1024];
	size_t bytes_read;
	DATA_SET *result = NULL;
	_PROCESS_PAIR pair;

	/* Allocate the data set */
	result = (DATA_SET*)calloc(1,sizeof(DATA_SET));
    result->inputCount = CalculateActualCount(norm,0,inputCount);
    result->idealCount = CalculateActualCount(norm,inputCount,outputCount);
	result->recordCount = norm->rowCount;
	result->data = (double*)calloc(norm->rowCount*(inputCount+outputCount+1),sizeof(double));
    result->cursor = result->data;

	/* Construct the process_pair, to pass to callbacks */
	pair.norm = norm;
	pair.data = result;

	/* Read and normalize file */
	if (csv_init(&p, CSV_APPEND_NULL) != 0) exit(EXIT_FAILURE);
	fp = fopen(filename, "rb");
	if (!fp)
	{ 
		printf("Could not open: %s\n", filename);
		exit(EXIT_FAILURE); 
	}

	norm->_currentColumn = 0;

	/* Read the file */
	while ((bytes_read=fread(buf, 1, 1024, fp)) > 0)
		if (csv_parse(&p, buf, bytes_read, _ProcessCallbackColumn, _ProcessCallbackRow, &pair) != bytes_read) {
			fprintf(stderr, "Error while parsing file: %s\n",
			csv_strerror(csv_error(&p)) );
			exit(EXIT_FAILURE);
		}

	/* Handle any final data.  May call the callbacks once more */
	csv_fini(&p, _ProcessCallbackColumn, _ProcessCallbackRow, &pair);

	/* Cleanup */
	fclose(fp);
	csv_free(&p);

	/* return the data set */
	return result;

}