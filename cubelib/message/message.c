#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "../include/data_type.h"
#include "../include/kernel_comp.h"
#include "../include/list.h"
#include "../include/string.h"
#include "../include/alloc.h"
#include "../include/json.h"
#include "../include/struct_deal.h"
//#include "../include/valuelist.h"
#include "../include/basefunc.h"
#include "../include/memdb.h"
#include "../include/message.h"

#include "message_box.h"

BYTE Blob[4096];
char text[4096];

// message generate function

static  struct tag_msg_kits * msg_kits;

void * message_get_expand_template()
{
	return msg_kits->expand_head_template;
}

void * message_get_head(void * message)
{
	struct message_box * msg_box;

	msg_box=(struct message_box *)message;
	if(message==NULL)
		return NULL;
	return &msg_box->head;

}

int message_get_state(void * message)
{
	struct message_box * msg_box;

	msg_box=(struct message_box *)message;

	if(message==NULL)
		return -EINVAL;
	return msg_box->head.state;
}

void * message_get_activemsg(void * message)
{
	struct message_box * msg_box;
	int ret;
	if(message==NULL)
		return -EINVAL;
	msg_box=(struct message_box *)message;
	return msg_box->active_msg;
}

int message_get_flag(void * message)
{
	struct message_box * msg_box;
	int ret;

	msg_box=(struct message_box *)message;

	return msg_box->head.flag;
}
void * _msg_load_template(char * subtypename)
{
	int subtype;

	// load head template
	subtype=memdb_get_subtypeno(msg_kits->type,subtypename);
	if(subtype<0)
		return NULL;
	return memdb_get_template(msg_kits->type,subtype);	
}
int message_set_route(void * message,const char * route)
{
	struct message_box * msg_box;
	int ret;
	int len;
	if(message==NULL)
		return -EINVAL;
	msg_box=(struct message_box *)message;
	len=strlen(route);
	if(len<DIGEST_SIZE)
		Memcpy(&(msg_box->head.route),route,len+1);
	else
		Memcpy(&(msg_box->head.route),route,DIGEST_SIZE);
	return 0;
}

int msgfunc_init()
{
	int ret;
	ret=Galloc0(&msg_kits,sizeof(struct tag_msg_kits));
	if(msg_kits==NULL)
		return -ENOMEM;
	msg_kits->type=memdb_get_typeno("MESSAGE");	
	if(msg_kits->type<0)
		return msg_kits->type;
	msg_kits->head_template =_msg_load_template("HEAD");
	if(msg_kits->head_template == NULL)
		return -EINVAL;
	msg_kits->expand_head_template =_msg_load_template("EXPAND");
	if(msg_kits->expand_head_template ==NULL)
		return -EINVAL;
	return 0;	
}

void * message_init()
{
	int ret;
	struct message_box * msg_box;
	ret=Galloc0(&msg_box,sizeof(struct message_box));
	if(ret<0)
		return NULL;
	Memcpy(msg_box->head.tag,"MESG",4);
	msg_box->head.version=0x00010001;
	msg_box->head_template=msg_kits->head_template;

	msg_box->box_state=MSG_BOX_INIT;
	return msg_box;
}

int __message_alloc_record_site(void * message)
{
	struct message_box * msg_box;
	int ret;
	MSG_HEAD * msg_head;

	msg_box=(struct message_box *)message;

	msg_head=&(msg_box->head);
	if(message==NULL)
		return -EINVAL;
	
	// malloc a new record_array space,and duplicate the old record_array value
	if(msg_head->record_num>0)
	{
		ret=Galloc0(&msg_box->record,(sizeof(BYTE *)+sizeof(void *)+sizeof(int))*msg_head->record_num);
		if(msg_box->record==NULL)
			return -ENOMEM;
		memset(msg_box->record,0,(sizeof(BYTE *)+sizeof(void *)+sizeof(int))*msg_box->head.record_num);

		msg_box->precord=msg_box->record+msg_head->record_num;
		msg_box->record_size=msg_box->precord+msg_head->record_num;
	}
	return 0;
}

int __message_alloc_expand_site(void * message)
{
	struct message_box * msg_box;
	int ret;
	MSG_HEAD * msg_head;

	msg_box=(struct message_box *)message;

	msg_head=&(msg_box->head);
	if(message==NULL)
		return -EINVAL;
	
	// malloc a new record_array space,and duplicate the old record_array value
	if(msg_head->expand_num>0)
	{
		ret=Galloc0(&msg_box->expand,(sizeof(BYTE *)+sizeof(void *)+sizeof(int))*msg_head->expand_num);
		if(msg_box->expand==NULL)
			return -ENOMEM;
		memset(msg_box->expand,0,(sizeof(BYTE *)+sizeof(void *)+sizeof(int))*msg_box->head.expand_num);

		msg_box->pexpand=msg_box->expand+msg_head->expand_num;
		msg_box->expand_size=msg_box->pexpand+msg_head->expand_num;
	}
	return 0;
}


int message_record_init(void * message)
{
        struct message_box * msg_box=message;
        MSG_HEAD * message_head;
        void * template;
        int record_size;
        int readbuf[1024];

        if((msg_box==NULL) || IS_ERR(msg_box))
            return -EINVAL;

        message_head=&(msg_box->head);
        msg_box->record_template=memdb_get_template(message_head->record_type,
		message_head->record_subtype);
        if(msg_box->record_template == NULL)
            return -EINVAL;
        if(IS_ERR(msg_box->record_template))
            return msg_box->record_template;

        __message_alloc_record_site(message);
        return 0;
}

int message_load_record_template(void * message)
{
        struct message_box * msg_box=message;
        MSG_HEAD * message_head;
	void * template;
	message_head=&(msg_box->head);
	if(msg_box->record_template!=NULL)
		return msg_box->record_template;
	return memdb_get_template(message_head->record_type,message_head->record_subtype);

}

void * message_create(int type,int subtype,void * active_msg)
{
	struct message_box * msg_box;
	MSG_HEAD * message_head;
	void * template;
	int record_size;
	int ret;

	msg_box=(struct message_box *)message_init();
	if((msg_box==NULL) || IS_ERR(msg_box))
		return -EINVAL;

	message_head=&msg_box->head;

	message_head->record_type=type;
	message_head->record_subtype=subtype;


   	msg_box->box_state=MSG_BOX_INIT;
	msg_box->head.state=MSG_BOX_INIT;

	ret=message_record_init(msg_box);
	if(ret<0)
	{
//		message_free(msg_box);
		return NULL;	
	}
	msg_box->active_msg=active_msg;
	return msg_box;
}

int __message_add_record_site(void * message,int increment)
{
	struct message_box * msg_box;
	int ret;
	MSG_HEAD * msg_head;
	BYTE * data;
	void * old_recordarray;
	void * old_precordarray;
	void * old_sizearray;
	

	msg_box=(struct message_box *)message;

	msg_head=&(msg_box->head);
	if(message==NULL)
		return -EINVAL;
	
	int record_no=msg_head->record_num;
	msg_head->record_num+=increment;

	old_recordarray=msg_box->record;
	old_precordarray=msg_box->precord;
	old_sizearray=msg_box->record_size;

	// malloc a new record_array space,and duplicate the old record_array value
	ret=Galloc0(&msg_box->record,(sizeof(BYTE *)+sizeof(void *)+sizeof(int))*msg_head->record_num);
	if(ret<0)
		return -ENOMEM;
	msg_box->precord=msg_box->record+msg_head->record_num;
	msg_box->record_size=msg_box->precord+msg_head->record_num;

	if(record_no>0)
	{
		memcpy(msg_box->record,old_recordarray,record_no*sizeof(BYTE *));
		memcpy(msg_box->precord,old_precordarray,record_no*sizeof(void *));
		memcpy(msg_box->record_size,old_sizearray,record_no*sizeof(int));
		// free the old record array,use new record to replace it
		Free(old_recordarray);
	}
	return 0;
}

int message_add_record(void * message,void * record)
{
	struct message_box * msg_box;
	int ret;
	MSG_HEAD * msg_head;
        int curr_site;

	msg_box=(struct message_box *)message;

	msg_head=&(msg_box->head);
	if(message==NULL)
		return -EINVAL;
	if(record==NULL)
		return -EINVAL;
	
    	int record_no=msg_head->record_num;

    	for(curr_site=0;curr_site<record_no;curr_site++)
    	{
        	if((msg_box->precord[curr_site]==NULL)
                    	&&(msg_box->record[curr_site]==NULL))
        	break;
    	}
    	if(curr_site==record_no)
        	ret=__message_add_record_site(message,1);
    	if(ret<0)
	    	return ret;

	// assign the record's value 
    	msg_box->precord[curr_site]=record;
    	msg_box->box_state=MSG_BOX_ADD;
    	return ret;
}

int message_record_struct2blob(void * message)
{
	struct message_box * msg_box;
	int ret;
	int i;
	BYTE * buffer;
	const int bufsize=4096;
	MSG_HEAD * msg_head;
	int blobsize;

	int record_size;
	msg_box=(struct message_box *)message;

	msg_head=&(msg_box->head);
	if(message==NULL)
		return -EINVAL;

	ret=Galloc0(&buffer,bufsize);
	if(ret<0)
		return -ENOMEM;

	record_size=0;
	for(i=0;i<msg_head->record_num;i++)
	{
		if(msg_box->record[i]==NULL)
		{
			if(msg_box->precord[i]==NULL)
				return -EINVAL;
			blobsize=struct_2_blob(msg_box->precord[i],buffer,msg_box->record_template);
			if(blobsize<0)
			{
				Free(buffer);
				return blobsize;
			}
			msg_box->record_size[i]=blobsize;
			ret=Galloc0(&msg_box->record[i],blobsize);
			if(msg_box->record[i]==NULL)
			{
				Free(buffer);
				return -ENOMEM;
			}
			Memcpy(msg_box->record[i],buffer,msg_box->record_size[i]);
		}
		record_size+=msg_box->record_size[i];
	}
	Free(buffer);
	msg_box->head.record_size=record_size;
	return record_size;
}

int message_expand_struct2blob(void * message)
{
	struct message_box * msg_box;
	int ret;
	int i;
	BYTE * buffer;
	const int bufsize=4096;
	MSG_HEAD * msg_head;
	int expand_size;

	msg_box=(struct message_box *)message;

	msg_head=&(msg_box->head);
	if(message==NULL)
		return -EINVAL;

	ret=Galloc0(&buffer,bufsize);
	if(buffer==NULL)
		return -ENOMEM;
	expand_size=0;
	for(i=0;i<msg_head->expand_num;i++)
	{
		if(msg_box->pexpand[i]==NULL)
		{
			if(msg_box->expand[i]==NULL)
				return -EINVAL;
			MSG_EXPAND * curr_expand=(MSG_EXPAND *)(msg_box->expand[i]);
			msg_box->expand_size[i]=curr_expand->data_size;
			expand_size+=msg_box->expand_size[i];
			continue;
		}

		MSG_EXPAND * curr_expand=(MSG_EXPAND *)(msg_box->pexpand[i]);
		void * struct_template=memdb_get_template(curr_expand->type,curr_expand->subtype);
		if(struct_template==NULL)
			return -EINVAL;
		ret=struct_2_blob(msg_box->pexpand[i],buffer,struct_template);
		if(ret<0)
		{
			Free(buffer);
			return ret;
		}

		msg_box->expand_size[i]=ret;
		curr_expand->data_size=ret;
		ret=Galloc0(&msg_box->expand[i],ret);
		if(msg_box->expand[i]==NULL)
		{
			Free(buffer);
			return -ENOMEM;
		}
		Memcpy(msg_box->expand[i],buffer,msg_box->expand_size[i]);
		Memcpy(msg_box->expand[i],&ret,sizeof(int));
		expand_size+=msg_box->expand_size[i];
	}
	Free(buffer);
	msg_box->head.expand_size=expand_size;
	return expand_size;
}

int message_output_blob(void * message, BYTE ** blob)
{
	struct message_box * msg_box;
	int ret;
	MSG_HEAD * msg_head;
	BYTE * data;
	BYTE * buffer;
	int i,j;
	int record_size,expand_size;
	int head_size;
	int blob_size,offset;
	

	msg_box=(struct message_box *)message;

	if(message==NULL)
		return -EINVAL;
	if(blob==NULL)
		return -EINVAL;

	record_size=0;
	expand_size=0;
	ret=Galloc0(&buffer,4096);
	if(buffer==NULL)
		return -ENOMEM;

	int flag=message_get_flag(message);
	if(flag &MSG_FLAG_CRYPT)
	{
		if(msg_box->blob == NULL)
			return -EINVAL;
	}
	if(msg_box->record_template == NULL)
	{
		if(msg_box->blob == NULL)
			return -EINVAL;
	}

	if(msg_box->head.record_num<0)
		return -EINVAL;
	if(msg_box->head.expand_num<0)
		return -EINVAL;

	offset=sizeof(MSG_HEAD);

	// duplicate record blob
	if(msg_box->blob != NULL)
	{
		Memcpy(buffer+offset,msg_box->blob,msg_box->head.record_size);
		offset+=msg_box->head.record_size;
	}	
	else 
	{
		ret=message_record_struct2blob(message);
		if(ret<0)
		{
			Free(buffer);
			return ret;
		}
		for(i=0;i<msg_box->head.record_num;i++)
		{
			Memcpy(buffer+offset,msg_box->record[i],msg_box->record_size[i]);
			offset+=msg_box->record_size[i];
		}
	}

	// duplicate expand blob
	ret=message_expand_struct2blob(message);
	if(ret<0)
		return ret;

	for(i=0;i<msg_box->head.expand_num;i++)
	{
		Memcpy(buffer+offset,msg_box->expand[i],msg_box->expand_size[i]);
		offset+=msg_box->expand_size[i];
	}

	Memcpy(buffer,&msg_box->head,sizeof(MSG_HEAD));

	blob_size=sizeof(MSG_HEAD)+msg_box->head.record_size+msg_box->head.expand_size;
	msg_box->blob=buffer;
	ret=Galloc0(blob,blob_size);
	if(ret<0)
		return ret;
	Memcpy(*blob,buffer,blob_size);
	
	Free(buffer);
	return blob_size;
}

int message_output_json(void * message, char * text)
{
	struct message_box * msg_box;
	int ret;
	MSG_HEAD * msg_head;
	BYTE * data;
	BYTE * buffer;
	int i,j;
	int record_size,expand_size;
	int head_size;
	int text_size,offset;
	
	msg_box=(struct message_box *)message;

	if(message==NULL)
		return -EINVAL;
	if(text==NULL)
		return -EINVAL;
	msg_head=&msg_box->head;
	
	buffer=Talloc(512);
	if(buffer==NULL)
		return -EINVAL;
	// output head text
	Strcpy(text,"{ \"Head\":");
	offset=Strlen(text);
	ret=struct_2_json(msg_head,text+offset,msg_kits->head_template);
	if(ret<0)
		return ret;
	offset+=ret;
	// output record text
	Strcpy(buffer,",\"Record\":[");
	ret=Strlen(buffer);
	Strcpy(text+offset,buffer);
	offset+=ret;	

	for(i=0;i<msg_head->record_num;i++)
	{
		if(i>0)
			text[offset++]=',';
		
		ret=struct_2_json(msg_box->precord[i],text+offset,msg_box->record_template);
		if(ret<0)
			return ret;
		offset+=ret;
	}
	
	Strcpy(buffer,"],\"Expand\" :[");
	Strcpy(text+offset,buffer);
	offset+=Strlen(buffer);
	for(i=0;i<msg_head->expand_num;i++)
	{
		MSG_EXPAND * expand;
		expand=msg_box->pexpand[i];
		void * expand_template=memdb_get_template(expand->type,
			expand->subtype);
		if(expand_template==NULL)
			return -EINVAL;
		ret=struct_2_json(msg_box->pexpand[i],text+offset,expand_template);
		if(ret<0)
			return ret;
		offset+=ret;
	}
	Strcpy(buffer,"]}");
	Strcpy(text+offset,buffer);
	offset+=Strlen(buffer);

	Free(buffer);
	return offset;
}

int message_read_head(void ** message,void * blob,int blob_size)
{
	struct message_box * msg_box;
	int ret;
	MSG_HEAD * msg_head;
	BYTE * data;
	BYTE * buffer;
	int i,j;
	int record_size,expand_size;
	int head_size;
	int current_offset;
	int total_size;
	

	if(message==NULL)
		return -EINVAL;
        if(blob_size<sizeof(MSG_HEAD))
        	return -EINVAL;
	if(blob==NULL)
		return -EINVAL;

	msg_box=message_init("MESG",0x00010001);
	if(msg_box==NULL)
		return -EINVAL;

	if(msg_box->box_state != MSG_BOX_INIT)
		return -EINVAL;

	head_size=blob_2_struct(blob,&(msg_box->head),msg_box->head_template);
	if(head_size<=0)
		return -EINVAL;
	msg_box->head_size=head_size;
        msg_box->current_offset=0;

	// check the head value
	if(msg_box->head.record_num<=0)
		return -EINVAL;
	if(msg_box->head.expand_num<0)
		return -EINVAL;
	if(msg_box->head.expand_num>MAX_EXPAND_NUM)
		return -EINVAL;

	if(strncmp(msg_box->head.tag,"MESG",4)!=0)
		return -EINVAL;
	if(msg_box->head.version!=0x00010001)
		return -EINVAL;

    	msg_box->box_state=MSG_BOX_LOADDATA;

   	total_size=msg_box->head.record_size+msg_box->head.expand_size;
	data=kmalloc(total_size,GFP_KERNEL);
	if(data==NULL)
		return -ENOMEM;
   	 msg_box->blob=data;
	 *message=msg_box;
   	 return sizeof(MSG_HEAD);
}

int  message_read_data(void * message,void * blob,int data_size)
{
	struct message_box * msg_box;
	int ret;
	MSG_HEAD * msg_head;
	BYTE * data;
	int new_size;
	int current_offset;
	int total_size;

	msg_box=(struct message_box *)message;
        if(msg_box->box_state!=MSG_BOX_LOADDATA)
                return -EINVAL;

	if(message==NULL)
		return -EINVAL;
	if(blob==NULL)
		return -EINVAL;
	
	if(data_size<=0)
		return -EINVAL;

	data=msg_box->blob;

        total_size=msg_box->head.record_size+msg_box->head.expand_size;

        if(data_size+msg_box->current_offset>=total_size)
	{
		new_size=total_size-msg_box->current_offset;
		memcpy(data+msg_box->current_offset,blob,new_size);
		msg_box->current_offset=0;
                current_offset=msg_box->head.record_size;
		int i;
		for(i=0;i<msg_box->head.expand_num;i++)
		{
			msg_box->expand[i]=data+current_offset;
			msg_box->expand_size[i]=*(int *)(data+current_offset);
			current_offset+=msg_box->expand_size[i];
		}
                __message_alloc_record_site(msg_box);
                msg_box->current_offset=0;
                msg_box->box_state=MSG_BOX_REBUILDING;
		return new_size;
	}	

	memcpy(data+msg_box->current_offset,blob,data_size);
	msg_box->current_offset+=data_size;
	return 0;
}

int message_read_from_blob(void ** message,void * blob, int data_size)
{
    const int buf_size=1024;
    int offset=0;
    int retval;
    int left_size;

    retval=message_read_head(message,blob,data_size);
    if(retval<0)
        return retval;
    offset=retval;
    left_size=data_size-retval;
    retval=message_read_data(*message,blob+offset,left_size);
    if(retval<0)
        return retval;
    return retval+offset;
}

int message_read_from_src(void ** message,void * src,
           int (*read_func)(void *,char *,int size))
{
    const int fixed_buf_size=4096;
    char readbuf[fixed_buf_size];
    struct message_box *message_box;
    MSG_HEAD * message_head;
    int read_size;
    int seek_size;
    int offset=0;
    int ret;
    int retval;
    int message_size;


    seek_size=0;
    ret=read_func(src,readbuf,sizeof(MSG_HEAD));
    if(ret<sizeof(MSG_HEAD))
        return ret;

    retval=message_read_head(message,readbuf,ret);
    if(retval<0)
         return -EINVAL;
     seek_size=sizeof(MSG_HEAD);
     message_head=message_get_head(*message);
   
     message_box=(struct message_box *)(*message);

    message_size = message_head->record_size+message_head->expand_size;

    while(offset<message_size)
    {
        read_size=message_size-offset;
        if(read_size>fixed_buf_size)
        {
            read_size=fixed_buf_size;
        }

        ret=read_func(src,readbuf,read_size);
        if(ret<=0)
        {
   //         message_free(*message);
            break;
        }
        retval=message_read_data(*message,readbuf,ret);
        if(retval<read_size)
            break;
        offset+=read_size;
        seek_size+=read_size;
    }
    return seek_size;
}

int message_load_record(void * message)
// this function read all the record from blob, and output these record
// struct to the precord point in the message_box
{

    struct message_box * msg_box;
    int ret;
    MSG_HEAD * msg_head;
    BYTE * data;
    BYTE * buffer;
    int i,j;
    int record_size,expand_size;
    int head_size;
    int no;

    struct struct_elem_attr * record_desc;

    msg_box=(struct message_box *)message;

    if(message==NULL)
        return -EINVAL;

// if(msg_box->box_state!=MSG_BOX_REBUILDING)
//       return -EINVAL;

    // choose the record's type
    if(msg_box->record_template==NULL)
    {
        ret= message_load_record_template(message);
        if(ret!=0)
            return ret;
    }


//  __message_alloc_record_site(msg_box);
//  msg_box->current_offset=msg_box->head_size;

    no=0;
    while(msg_box->record[no]!=NULL)
    {
        if(no>=msg_box->head.record_num)
            return 0;
        no++;
    }

    for(;no<msg_box->head.record_num;no++)
    {
        void * record;
        msg_box->record[no]=(BYTE *)(msg_box->blob)+msg_box->current_offset;
        ret=message_get_record(msg_box,&record,no);
        if(ret<0)
            return ret;
        if(record==NULL)
            return -EINVAL;
        msg_box->current_offset+=msg_box->record_size[no];
    }

//	record_size=blob_2_struct((BYTE *)(msg_box->record[no]),record,msg_box->record_template);
    if(ret<0)
        return ret;
    msg_box->current_offset+=msg_box->record_size[no];
//	msg_box->record_size[no]=record_size;
    return 1;
}

int message_load_expand(void * message)
// this function read all the expand from blob, and output these expand 
// struct to the pexpand point in the message_box
{

    struct message_box * msg_box;
    int ret;
    MSG_HEAD * msg_head;
    MSG_EXPAND * expand;
    BYTE * data;
    BYTE * buffer;
    int i,j;
    int expand_size;
    int head_size;
    int no;
    int offset;
    void * struct_template;
    void * expand_struct;

    msg_box=(struct message_box *)message;

    if(message==NULL)
        return -EINVAL;

//    if(msg_box->box_state!=MSG_BOX_REBUILDING)
//        return -EINVAL;

    // choose the record's type

    no=0;
    offset=msg_box->head.record_size;

    data=msg_box->blob+offset;
    offset=0;
    for(;no<msg_box->head.expand_num;no++)
    {
	if(offset>=msg_box->head.expand_size)
		return -EINVAL;
	    
	expand=(MSG_EXPAND *)data;
	if(expand->data_size<0)
		return -EINVAL;
	offset+=expand->data_size;
	struct_template=memdb_get_template(expand->type,expand->subtype);
	if(struct_template==NULL)
	{
		data+=expand->data_size;
		continue;
	}
	buffer=Talloc(struct_size(struct_template));
	if(buffer==NULL)
		return -EINVAL;
	ret=blob_2_struct(data,buffer,struct_template);
	if(ret!=expand->data_size)
	{
		struct_free(buffer,struct_template);
		return -EINVAL;
	}
	msg_box->pexpand[no]=buffer;
	data+=expand->data_size;

    }
    return offset;
}


const char * message_get_receiver(void * message)
{
	struct message_box * msg_box;
	int ret;
	if(message==NULL)
		return NULL;
	msg_box=(struct message_box *)message;
	return &(msg_box->head.receiver_uuid);
}

void message_free(void * message)
{
	struct message_box * msg_box;
	int i;
	msg_box=(struct message_box *)message;
	if(message==NULL)
		return ;

	for(i=0;i<msg_box->head.expand_num;i++)
	{
		if(msg_box->expand[i]!=NULL)
                {
			free(msg_box->expand[i]);
                    	msg_box->expand[i]=NULL;
                }
                if(msg_box->pexpand[i]!=NULL)
                {
                    free(msg_box->pexpand[i]);
                    msg_box->pexpand[i]=NULL;
                }
       }
       for(i=0;i<msg_box->head.record_num;i++)
       {
        	if(msg_box->record[i]!=NULL)
                {
                    free(msg_box->record[i]);
                    msg_box->record[i]=NULL;
                }
                if(msg_box->precord[i]!=NULL)
                {
                    free(msg_box->precord[i]);
                    msg_box->precord[i]=NULL;
                }
	}

        free(msg_box->record);
        free_struct_template(msg_box->record_template);
    	free_struct_template(msg_box->head_template);
    	free(msg_box);
	return;
}

const char * message_get_recordtype(void * message)
{
	struct message_box * msg_box;
	int ret;
	if(message==NULL)
		return NULL;
	msg_box=(struct message_box *)message;
	return &(msg_box->head.record_type);
}

const char * message_get_sender(void * message)
{
	struct message_box * msg_box;
	int ret;
	if(message==NULL)
		return NULL;
	msg_box=(struct message_box *)message;
	return &(msg_box->head.sender_uuid);
}


int message_set_receiver(void * message,const char * receiver_uuid)
{
	struct message_box * msg_box;
	int ret;
	int len;
	if(message==NULL)
		return -EINVAL;
	msg_box=(struct message_box *)message;
	len=strlen(receiver_uuid);
	if(len<DIGEST_SIZE*2)
		Memcpy(&(msg_box->head.receiver_uuid),receiver_uuid,len+1);
	else
		Memcpy(&(msg_box->head.receiver_uuid),receiver_uuid,DIGEST_SIZE*2);
	return 0;
}

/*
int message_set_head(void * message,char * item_name, void * value)
{
	struct message_box * msg_box;
	int ret;

	msg_box=(struct message_box *)message;

//	msg_box->box_state=MSG_BOX_DEAL;
	if(message==NULL)
		return -EINVAL;
	if(value==NULL)
		return -EINVAL;
	if(msg_box->head_template ==NULL)
		return -EINVAL;

	ret=struct_write_elem(item_name,&(msg_box->head),value,msg_box->head_template);
	return ret;
}

int read_message_head_elem(void * message,char * item_name, void * value)
{
    struct message_box * msg_box;
    int ret;

    msg_box=(struct message_box *)message;

    if(message==NULL)
        return -EINVAL;
    if(value==NULL)
        return -EINVAL;
    if(msg_box->head_template ==NULL)
        return -EINVAL;

    ret=struct_read_elem(item_name,&(msg_box->head),value,msg_box->head_template);
    ((char *)value)[ret]=0;
    return ret;
}
int message_comp_head_elem_text(void * message,char * item_name, char * text)
{
    struct message_box * msg_box;
    int ret;

    msg_box=(struct message_box *)message;

    if(message==NULL)
        return -EINVAL;
    if(text==NULL)
        return -EINVAL;
    if(msg_box->head_template ==NULL)
        return -EINVAL;

    ret=struct_comp_elem_text(item_name,&(msg_box->head),text,msg_box->head_template);
    return ret;
}
*/
int message_read_elem(void * message,char * item_name, int index, void ** value)
{
    struct message_box * msg_box;
    int ret;
    char buffer[128];

    msg_box=(struct message_box *)message;

    if(message==NULL)
        return -EINVAL;
    if(value==NULL)
        return -EINVAL;
    if(msg_box->head_template ==NULL)
        return -EINVAL;
    if(msg_box->record_template ==NULL)
    {
	 msg_box->record_template=memdb_get_template(msg_box->head.record_type,msg_box->head.record_subtype);
        if(msg_box->record_template==NULL)
       		return -EINVAL;
    }
    if(index>=msg_box->head.record_num)
   	return -EINVAL;
    if(index<0)
	 return -EINVAL;
    if(msg_box->precord[index]==NULL)
	    return -EINVAL;
	
    ret=struct_read_elem(item_name,msg_box->precord[index],buffer,msg_box->record_template);
    if(ret<0)
	    return ret;
    if(ret>=128)
	    return -EINVAL;
    *value=malloc(ret+1);
    memcpy(*value,buffer,ret+1);
    return ret;
}
/*
int message_comp_elem_text(void * message,char * item_name, int index, char * text)
{
    struct message_box * msg_box;
    int ret;
    char buffer[128];

    msg_box=(struct message_box *)message;

    if(message==NULL)
        return -EINVAL;
    if(text==NULL)
        return -EINVAL;
    if(msg_box->head_template ==NULL)
        return -EINVAL;
    if(msg_box->record_template ==NULL)
    {
	 msg_box->record_template=load_record_template(msg_box->head.record_type);
        if(msg_box->record_template==NULL)
       		return -EINVAL;
    }
    if(index>=msg_box->head.record_num)
   	return -EINVAL;
    if(index<0)
	 return -EINVAL;
    if(msg_box->precord[index]==NULL)
	    return -EINVAL;
	
    ret=struct_comp_elem_text(item_name,msg_box->precord[index],text,msg_box->record_template);
    return ret;
}
*/
int message_set_flag(void * message, int flag)
{
	struct message_box * msg_box;
	int ret;

	msg_box=(struct message_box *)message;

	if(message==NULL)
		return -EINVAL;
	msg_box->head.flag=flag;
	return 0;
}
int message_set_state(void * message, int state)
{
	struct message_box * msg_box;
	int ret;

	msg_box=(struct message_box *)message;

	if(message==NULL)
		return -EINVAL;
	msg_box->head.state=state;
	return 0;
}

int message_set_flow(void * message, int flow)
{
	struct message_box * msg_box;
	int ret;

	msg_box=(struct message_box *)message;

	if(message==NULL)
		return -EINVAL;
	msg_box->head.flow=flow;
	return 0;
}

int message_get_flow(void * message)
{
	struct message_box * msg_box;

	msg_box=(struct message_box *)message;

	if(message==NULL)
		return -EINVAL;
	return msg_box->head.flow;
}

/*
int __message_alloc_record_site(void * message)
{
	struct message_box * msg_box;
	int ret;
	MSG_HEAD * msg_head;

	msg_box=(struct message_box *)message;

	msg_head=&(msg_box->head);
	if(message==NULL)
		return -EINVAL;
	
	// malloc a new record_array space,and duplicate the old record_array value
	msg_box->record=kmalloc((sizeof(BYTE *)+sizeof(void *)+sizeof(int))*msg_head->record_num,GFP_KERNEL);
	if(msg_box->record==NULL)
		return -ENOMEM;
	memset(msg_box->record,0,(sizeof(BYTE *)+sizeof(void *)+sizeof(int))*msg_box->head.record_num);
	msg_box->precord=msg_box->record+msg_head->record_num;
	msg_box->record_size=msg_box->precord+msg_head->record_num;
	return 0;
}

int message_record_init(void * message)
{
        struct message_box * msg_box=message;
        MSG_HEAD * message_head;
        void * template;
        int record_size;
        int readbuf[1024];

        if((msg_box==NULL) || IS_ERR(msg_box))
            return -EINVAL;

        message_head=&(msg_box->head);
        msg_box->record_template=load_record_template(message_head->record_type);
        if(msg_box->record_template == NULL)
            return -EINVAL;
        if(IS_ERR(msg_box->record_template))
            return msg_box->record_template;

        __message_alloc_record_site(message);
        return 0;
}

int __message_add_record_site(void * message,int increment)
{
	struct message_box * msg_box;
	int ret;
	MSG_HEAD * msg_head;
	BYTE * data;
	void * old_recordarray;
	void * old_precordarray;
	void * old_sizearray;
	

	msg_box=(struct message_box *)message;

	msg_head=&(msg_box->head);
	if(message==NULL)
		return -EINVAL;
	
	int record_no=msg_head->record_num;
	msg_head->record_num+=increment;

	old_recordarray=msg_box->record;
	old_precordarray=msg_box->precord;
	old_sizearray=msg_box->record_size;

	// malloc a new record_array space,and duplicate the old record_array value
	msg_box->record=kmalloc((sizeof(BYTE *)+sizeof(void *)+sizeof(int))*msg_head->record_num,GFP_KERNEL);
	if(msg_box->record==NULL)
		return -ENOMEM;
	memset(msg_box->record,0,(sizeof(BYTE *)+sizeof(void *)+sizeof(int))*msg_box->head.record_num);
	msg_box->precord=msg_box->record+msg_head->record_num;
	msg_box->record_size=msg_box->precord+msg_head->record_num;

	if(record_no>0)
	{
		memcpy(msg_box->record,old_recordarray,record_no*sizeof(BYTE *));
		memcpy(msg_box->precord,old_precordarray,record_no*sizeof(void *));
		memcpy(msg_box->record_size,old_sizearray,record_no*sizeof(int));
		// free the old record array,use new record to replace it
		kfree(old_recordarray);
	}
	return 0;
}

// message add functions

int message_add_record_blob(void * message,int record_size, BYTE * record)
{
	struct message_box * msg_box;
	int ret;
	MSG_HEAD * msg_head;
	BYTE * data;
    int curr_site;
	
	msg_box=(struct message_box *)message;

	msg_head=&(msg_box->head);
	if(message==NULL)
		return -EINVAL;
	if(record==NULL)
		return -EINVAL;
	
//	if(msg_box->box_state!=MSG_BOX_ADD_RECORD)
//		return -EINVAL;
	
	int record_no=msg_head->record_num;

    for(curr_site=0;curr_site<record_no;curr_site++)
    {
        if((msg_box->precord[curr_site]==NULL)
                    &&(msg_box->record[curr_site]==NULL))
        break;
    }
    if(curr_site==record_no)
        __message_add_record_site(message,1);

    // malloc new record's space
	data=kmalloc(record_size,GFP_KERNEL);
	if(data==NULL)
		return -ENOMEM;
	memcpy(data,record,record_size);
	msg_box->record[record_no]=data;

	msg_box->record_size[record_no]=record_size;
	msg_box->head.record_size+=record_size;
    msg_box->box_state=MSG_BOX_ADD;
	return ret;
}



int message_add_record(void * message,void * record)
{
	struct message_box * msg_box;
	int ret;
	MSG_HEAD * msg_head;
        int curr_site;

	msg_box=(struct message_box *)message;

	msg_head=&(msg_box->head);
	if(message==NULL)
		return -EINVAL;
	if(record==NULL)
		return -EINVAL;
	
    int record_no=msg_head->record_num;

    for(curr_site=0;curr_site<record_no;curr_site++)
    {
        if((msg_box->precord[curr_site]==NULL)
                    &&(msg_box->record[curr_site]==NULL))
        break;
    }
    if(curr_site==record_no)
        ret=__message_add_record_site(message,1);
    if(ret<0)
	    return ret;

	// assign the record's value 
    msg_box->precord[curr_site]=record;
    msg_box->box_state=MSG_BOX_ADD;
    return ret;
}
*/
int message_add_expand(void * message,void * expand)
{
	struct message_box * msg_box;
	int ret;
	MSG_HEAD * msg_head;

	msg_box=(struct message_box *)message;

	msg_head=&(msg_box->head);
	if(message==NULL)
		return -EINVAL;
	if(expand==NULL)
		return -EINVAL;
	msg_box->pexpand[msg_head->expand_num++]=expand;
        msg_box->box_state=MSG_BOX_EXPAND;

	return ret;
}
/*
int message_add_expand_blob(void * message,void * expand)
{
	struct message_box * msg_box;
	int ret;
	MSG_HEAD * msg_head;
	MSG_EXPAND * expand_head;

	msg_box=(struct message_box *)message;

	msg_head=&(msg_box->head);
	if(message==NULL)
		return -EINVAL;
	if(expand==NULL)
		return -EINVAL;
	expand_head=(MSG_EXPAND *)expand;
	if(expand_head->data_size<0)
		return -EINVAL;
	if(expand_head->data_size>4096)
		return -EINVAL;
	msg_box->expand[msg_head->expand_num++]=expand;
        msg_box->box_state=MSG_BOX_EXPAND;

	return ret;
}


int message_record_blob2struct(void * message)
{
	struct message_box * msg_box;
	int ret;
	int i;
	MSG_HEAD * msg_head;

	msg_box=(struct message_box *)message;

	msg_head=&(msg_box->head);
	if(message==NULL)
		return -EINVAL;
	for(i=0;i<msg_head->record_num;i++)
	{
		if(msg_box->precord[i]==NULL)
		{
			if(msg_box->record[i]==NULL)
				return -EINVAL;
			ret=alloc_struct(&(msg_box->precord[i]),msg_box->record_template);
			if(ret<0)
				return ret;
			ret=blob_2_struct(msg_box->record[i],msg_box->precord[i],msg_box->record_template);
			if(ret!=msg_box->record_size[i])
				return -EINVAL;
		}
	}
	return 0;
}

int message_record_struct2blob(void * message)
{
	struct message_box * msg_box;
	int ret;
	int i;
	BYTE * buffer;
	const int bufsize=65536;
	MSG_HEAD * msg_head;

	int record_size;
	msg_box=(struct message_box *)message;

	msg_head=&(msg_box->head);
	if(message==NULL)
		return -EINVAL;

	buffer=kmalloc(bufsize,GFP_KERNEL);
	if(buffer==NULL)
		return -ENOMEM;

	record_size=0;
	for(i=0;i<msg_head->record_num;i++)
	{
		if(msg_box->record[i]==NULL)
		{
			if(msg_box->precord[i]==NULL)
				return -EINVAL;
			ret=struct_2_blob(msg_box->precord[i],buffer,msg_box->record_template);
			if(ret<0)
			{
				kfree(buffer);
				return ret;
			}
			msg_box->record_size[i]=ret;
			msg_box->record[i]=kmalloc(ret,GFP_KERNEL);
			if(msg_box->record[i]==NULL)
			{
				kfree(buffer);
				return -ENOMEM;
			}
			memcpy(msg_box->record[i],buffer,msg_box->record_size[i]);
		}
		record_size+=msg_box->record_size[i];
	}
	kfree(buffer);
	msg_box->head.record_size=record_size;
	return record_size;
}

int message_expand_struct2blob(void * message)
{
	struct message_box * msg_box;
	int ret;
	int i;
	BYTE * buffer;
	const int bufsize=65536;
	MSG_HEAD * msg_head;
	int expand_size;

	msg_box=(struct message_box *)message;

	msg_head=&(msg_box->head);
	if(message==NULL)
		return -EINVAL;

	buffer=kmalloc(bufsize,GFP_KERNEL);
	if(buffer==NULL)
		return -ENOMEM;
	expand_size=0;
	for(i=0;i<msg_head->expand_num;i++)
	{
		if(msg_box->pexpand[i]==NULL)
		{
			if(msg_box->expand[i]==NULL)
				return -EINVAL;
			MSG_EXPAND * curr_expand=(MSG_EXPAND *)(msg_box->expand[i]);
			msg_box->expand_size[i]=curr_expand->data_size;
			expand_size+=msg_box->expand_size[i];
			continue;
		}

		MSG_EXPAND * curr_expand=(MSG_EXPAND *)(msg_box->pexpand[i]);
		void * struct_template=load_record_template(curr_expand->tag);
		if(struct_template==NULL)
			return -EINVAL;
		ret=struct_2_blob(msg_box->pexpand[i],buffer,struct_template);
		free_struct_template(struct_template);
		if(ret<0)
		{
			kfree(buffer);
			return ret;
		}

		msg_box->expand_size[i]=ret;
		curr_expand->data_size=ret;
		msg_box->expand[i]=kmalloc(ret,GFP_KERNEL);
		if(msg_box->expand[i]==NULL)
		{
			kfree(buffer);
			return -ENOMEM;
		}
		memcpy(msg_box->expand[i],buffer,msg_box->expand_size[i]);
		memcpy(msg_box->expand[i],&ret,sizeof(int));
		expand_size+=msg_box->expand_size[i];
	}
	kfree(buffer);
	msg_box->head.expand_size=expand_size;
	return expand_size;
}


int message_output_blob(void * message, BYTE ** blob)
{
	struct message_box * msg_box;
	int ret;
	MSG_HEAD * msg_head;
	BYTE * data;
	BYTE * buffer;
	int i,j;
	int record_size,expand_size;
	int head_size;
	int blob_size,offset;
	

	msg_box=(struct message_box *)message;

	if(message==NULL)
		return -EINVAL;
	if(blob==NULL)
		return -EINVAL;
	
	record_size=0;
	expand_size=0;
	buffer=kmalloc(4096,GFP_KERNEL);
	if(buffer==NULL)
		return -ENOMEM;

	int flag=message_get_flag(message);
	if(flag &MSG_FLAG_CRYPT)
	{
		if(msg_box->blob == NULL)
			return -EINVAL;
	}
	if(msg_box->record_template == NULL)
	{
		if(msg_box->blob == NULL)
			return -EINVAL;
	}

	if(msg_box->head.record_num<0)
		return -EINVAL;
	if(msg_box->head.expand_num<0)
		return -EINVAL;
	if(msg_box->head.expand_num>MAX_EXPAND_NUM)
		return -EINVAL;

	offset=sizeof(MSG_HEAD);

	// duplicate record blob
	if(msg_box->blob != NULL)
	{
		Memcpy(buffer+offset,msg_box->blob,msg_box->head.record_size);
		offset+=msg_box->head.record_size;
	}	
	else 
	{
		ret=message_record_struct2blob(message);
		if(ret<0)
		{
			free(buffer);
			return ret;
		}
		for(i=0;i<msg_box->head.record_num;i++)
		{
			memcpy(buffer+offset,msg_box->record[i],msg_box->record_size[i]);
			offset+=msg_box->record_size[i];
		}
	}

	// duplicate expand blob
	ret=message_expand_struct2blob(message);
	if(ret<0)
		return ret;



	for(i=0;i<msg_box->head.expand_num;i++)
	{
		memcpy(buffer+offset,msg_box->expand[i],msg_box->expand_size[i]);
		offset+=msg_box->expand_size[i];
	}

	head_size=struct_2_blob(&(msg_box->head),buffer,msg_box->head_template);
	if(head_size!=sizeof(MSG_HEAD))
	{
		free(buffer);
		return -EINVAL;
	}
	blob_size=head_size+msg_box->head.record_size+msg_box->head.expand_size;
	msg_box->blob=buffer;
	*blob=msg_box->blob;
	return blob_size;
}

int message_output_record_blob(void * message, BYTE ** blob)
{
	struct message_box * msg_box;
	int ret;
	MSG_HEAD * msg_head;
	BYTE * data;
	BYTE * buffer;
	int i,j;
	int record_size,expand_size;
	int head_size;
	int blob_size,offset;
	

	msg_box=(struct message_box *)message;

	if(message==NULL)
		return -EINVAL;
	if(blob==NULL)
		return -EINVAL;
	
	record_size=0;
	buffer=kmalloc(4096,GFP_KERNEL);
	if(buffer==NULL)
		return -ENOMEM;

	int flag=message_get_flag(message);
	if(flag &MSG_FLAG_CRYPT)
	{
		if(msg_box->blob == NULL)
			return -EINVAL;
	}
	if(msg_box->record_template == NULL)
	{
		if(msg_box->blob == NULL)
			return -EINVAL;
	}

	if(msg_box->head.record_num<0)
		return -EINVAL;

	offset=0;

	// duplicate record blob
	if(msg_box->blob != NULL)
	{
		Memcpy(buffer+offset,msg_box->blob,msg_box->head.record_size);
		offset+=msg_box->head.record_size;
	}	
	else 
	{
		ret=message_record_struct2blob(message);
		if(ret<0)
		{
			free(buffer);
			return ret;
		}
		for(i=0;i<msg_box->head.record_num;i++)
		{
			memcpy(buffer+offset,msg_box->record[i],msg_box->record_size[i]);
			offset+=msg_box->record_size[i];
		}
	}
	*blob=malloc(offset);
	if(*blob==NULL)
		return -ENOMEM;
	memcpy(*blob,buffer,offset);
	return offset;
}
*/
int message_get_record(void * message,void ** msg_record, int record_no)
{
	struct message_box * msg_box;
	MSG_HEAD * msg_head;
	void * struct_template;
	int ret;

	msg_box=(struct message_box *)message;

	if(message==NULL)
		return -EINVAL;
	*msg_record=NULL;
	
	msg_head=message_get_head(message);
	if((msg_head==NULL) || IS_ERR(msg_head))
		return -EINVAL;
	if(record_no<0)
		return -EINVAL;
	if(record_no>=msg_head->record_num)
		return 0;
	if(msg_box->record_template==NULL)
        	msg_box->record_template=memdb_get_template(msg_head->record_type,
			msg_head->record_subtype);
	if(msg_box->precord[record_no]==NULL)
	{

		ret=Galloc0(&(msg_box->precord[record_no]),msg_box->record_template);
		if(ret<0)
			return ret;
		if(msg_box->precord[record_no]==NULL)
			return -ENOMEM;	
		ret=blob_2_struct(msg_box->record[record_no],msg_box->precord[record_no],msg_box->record_template);
		if(ret<0)
		{
			struct_free(msg_box->precord[record_no],msg_box->record_template);
			return ret;
		}
		msg_box->record_size[record_no]=ret;
	}
	*msg_record=clone_struct(msg_box->precord[record_no],msg_box->record_template);
	if(*msg_record==NULL)
		return -EINVAL;
	return 0;
}
int message_get_expand(void * message,void ** msg_record, int record_no)
{
	struct message_box * msg_box;
	MSG_HEAD * msg_head;
	MSG_EXPAND * expand_head;
	void * struct_template;
	int ret;

	msg_box=(struct message_box *)message;

	if(message==NULL)
		return -EINVAL;
	*msg_record=NULL;
	
	msg_head=message_get_head(message);
	if((msg_head==NULL) || IS_ERR(msg_head))
		return -EINVAL;
	if(record_no<0)
		return -EINVAL;
	if(record_no>=msg_head->expand_num)
		return 0;
        struct_template=memdb_get_template(DTYPE_MESSAGE,SUBTYPE_EXPAND);
	if(msg_box->precord[record_no]==NULL)
	{

		ret=Galloc0(&expand_head,struct_size(struct_template));
		if(ret<0)
			return ret;
		ret=blob_2_struct(expand_head,msg_box->record[record_no],struct_template);
		if(ret<0)
			return ret;
		struct_template=memdb_get_template(expand_head->type,expand_head->subtype);
		if(struct_template==NULL)
			return -EINVAL;
		
		ret=Galloc0(&(msg_box->precord[record_no]),struct_size(struct_template));
		if(msg_box->precord[record_no]==NULL)
			return -ENOMEM;	
		ret=blob_2_struct(msg_box->precord[record_no],msg_box->record[record_no],struct_template);
		if(ret<0)
		{
			struct_free(msg_box->precord[record_no],msg_box->record_template);
			return ret;
		}
		msg_box->record_size[record_no]=ret;
	}
	else
	{
		expand_head=msg_box->precord[record_no];
		struct_template=memdb_get_template(expand_head->type,expand_head->subtype);
		if(struct_template==NULL)
			return -EINVAL;
	}
	*msg_record=clone_struct(msg_box->precord[record_no],msg_box->record_template);
	if(*msg_record==NULL)
		return -EINVAL;
	return 0;
}
int message_get_define_expand(void * message,void ** addr,int type,int subtype)
{
    	struct message_box * msg_box;
	int ret;
	MSG_HEAD * msg_head;
	BYTE * data;
	int struct_size;
	int i;
	MSG_EXPAND * define_expand=NULL;

	msg_box=(struct message_box *)message;

	if(message==NULL)
		return -EINVAL;
	*addr=NULL;

	for(i=0;i<msg_box->head.expand_num;i++)
	{
		if(msg_box->pexpand[i]==NULL)
			return 0;
		MSG_EXPAND *curr_expand=(MSG_EXPAND *)(msg_box->pexpand[i]);	

		if((curr_expand->type!=type)
			||(curr_expand->subtype!=subtype))
			continue;
		define_expand=curr_expand;
		break;
	}
	if(define_expand==NULL)
		return 0;
	*addr=define_expand;
	return i;
}

int message_replace_define_expand(void * message,void * addr,int type,int subtype)
{
    	struct message_box * msg_box;
	int ret;
	MSG_HEAD * msg_head;
	BYTE * data;
	int struct_size;
	int i;

	msg_box=(struct message_box *)message;
	MSG_EXPAND * temp_expand;

	if(message==NULL)
		return -EINVAL;

//:	msg_box->box_state=MSG_BOX_ADD_EXPAND;
	for(i=0;i<msg_box->head.expand_num;i++)
	{
		if(msg_box->pexpand[i]==NULL)
			return 0;
		MSG_EXPAND *curr_expand=(MSG_EXPAND *)(msg_box->pexpand[i]);	

		if((curr_expand->type!=type)
			||(curr_expand->subtype!=subtype))
			continue;
		break;
	}
	if(i==msg_box->head.expand_num)
		return 0;

	temp_expand=msg_box->pexpand[i];
	if(msg_box->expand[i]!=NULL)
	{
		msg_box->expand[i]=NULL;
		msg_box->expand_size[i]=0;
	}
	msg_box->pexpand[i]=addr;

	void * struct_template=memdb_get_template(type,subtype);
	if(struct_template!=NULL)
	{
		struct_free(temp_expand,struct_template);
	}
	return i;
}
int message_remove_indexed_expand(void * message, int expand_no,void **expand)
{
	struct message_box * msg_box;
	char type[5];
	int i,j;
	void * addr;
	int retval;
	MSG_EXPAND * expand_data;
	MSG_HEAD * msg_head;

	msg_box=(struct message_box *)message;

	if((message==NULL) || IS_ERR(message))
		return -EINVAL;
	if(expand_no<0)
		return -EINVAL;
	if(expand_no>=msg_box->head.expand_num)
		return -EINVAL;
	msg_head=&(msg_box->head);

	*expand=msg_box->pexpand[expand_no];
/*	
	if(msg_box->expand[expand_no]!=NULL)
	{
		free(msg_box->expand[expand_no]);
	}
*/
	for(i=expand_no;i<msg_head->expand_num;i++)
	{
		msg_box->expand[i]=msg_box->expand[i+1];
		msg_box->pexpand[i]=msg_box->pexpand[i+1];
		msg_box->expand_size[i]=msg_box->expand_size[i+1];
	}
	msg_head->expand_num--;
	return 0;
}
int message_remove_expand(void * message, int type,int subtype,void ** expand)
{
	struct message_box * msg_box;
	int i,j;
	void * addr;
	int retval;
	MSG_EXPAND * expand_data;

	msg_box=(struct message_box *)message;

	if((message==NULL) || IS_ERR(message))
		return -EINVAL;
	*expand=NULL;

	for(i=0;i<msg_box->head.expand_num;i++)
	{
		retval=message_get_expand(message,&expand_data,i);
		if(retval<0)
			continue;
		if(expand_data==NULL)
			return 0;
		if((expand_data->type==type) &&
			(expand_data->subtype==subtype))
		{
			retval=message_remove_indexed_expand(message,i,expand);
			break;
		}
	}

	return 0;
}

