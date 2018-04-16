#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <gst/gst.h>
#include <time.h>
#include <string.h>
#include <gst/app/gstappsink.h>
#include <opencv2/opencv.hpp>
#include <fcntl.h>  
#include <sys/stat.h>  

//#include <opencv2/core/core.hpp>




#define MAX_FILE_LEN 512

// TODO: use synchronized deque
//std::deque<cv::Mat> frameQueue;


typedef struct _CustomData 
{  
  GstElement *pipeline, *camera_source,*store_sink,*tee,*app_convert,*app_sink;
  GstElement *video_queue, *visual, *store_convert,*mux,*app_capsfilter;
  GstElement *app_queue, *encoder,*store_capsfilter,*store_queue;
  gboolean terminate;
} CustomData;

typedef struct _datetime_
{
	unsigned char year;
	unsigned char month;
	unsigned char day;
	unsigned char hour;
	unsigned char minute;
	unsigned char second;
} datetime_t;

typedef struct file_info_s
{
    datetime_t   starttime;
    datetime_t   endtime;
    char         filestartname[MAX_FILE_LEN];
	char         fileendname[MAX_FILE_LEN];
} file_info_t;


void ConvertDvrTime_tToDatetime(const time_t *timep, datetime_t *pDatetime)
{
	if (!pDatetime)
	{
		return;
	}

	struct tm mytm;
	struct tm* ptm = &mytm;
	memset(ptm, 0, sizeof(struct tm));
	gmtime_r(timep, ptm);
	if (ptm->tm_year > 99 )
	{
		pDatetime->year =  ptm->tm_year + 1900 - 2000 ;
	}
	else
	{
		pDatetime->year =  ptm->tm_year;
	}

	pDatetime->month = ptm->tm_mon + 1;
	pDatetime->day	 = ptm->tm_mday;
	pDatetime->hour  = ptm->tm_hour;
	pDatetime->minute = ptm->tm_min;
	pDatetime->second = ptm->tm_sec;
}

void GetSysTime(datetime_t* pstTime)
{
	time_t curtime;

	time(&curtime);
	ConvertDvrTime_tToDatetime(&curtime, pstTime);

	return;
}

void ConvertDvrDatetimeToTime_t(const datetime_t *pDatetime, time_t *timep)
{
    if (!timep)
    {
        return;
    }

    struct tm tmTime;
    tmTime.tm_sec  = pDatetime->second;
    tmTime.tm_min  = pDatetime->minute;
    tmTime.tm_hour = pDatetime->hour;
    tmTime.tm_mday = pDatetime->day;
    tmTime.tm_mon  = pDatetime->month - 1;
    if (pDatetime->year < 70)
    {
        tmTime.tm_year = pDatetime->year + 2000 - 1900;
    }
    else
    {
        tmTime.tm_year = pDatetime->year;
    }
    *timep = mktime(&tmTime);
}

int CompearTime(datetime_t *pTime1, datetime_t *pTime2)
{
    time_t time1, time2;
    int iDif = 0;

    ConvertDvrDatetimeToTime_t(pTime1, &time1);
    ConvertDvrDatetimeToTime_t(pTime2, &time2);


    iDif = (int)(time1 - time2);

    return iDif;
}

static void handle_message (CustomData *data, GstMessage *msg)
{
	GError *err;
	gchar *debug_info;

	switch (GST_MESSAGE_TYPE (msg))
	{
	case GST_MESSAGE_ERROR:
		gst_message_parse_error(msg, &err, &debug_info);
		g_printerr ("Error received from element %s: %s\n", GST_OBJECT_NAME (msg->src), err->message);
		g_printerr ("Debugging information: %s\n", debug_info ? debug_info : "none");
		g_clear_error (&err);
		g_free (debug_info);
		data->terminate = TRUE;
		break;
	case GST_MESSAGE_EOS:
		g_print ("End-Of-Stream reached.\n");
		data->terminate = TRUE;
		break;
	default:
		g_printerr ("Unexpected message received.\n");  
		break;  
	}
	gst_message_unref (msg);
}  

void getfilename(datetime_t *starttime,char* buffer)
{
	sprintf(buffer, "save/%02d%02d%02d-%02d%02d%02d-%02d%02d%02d.264",
                starttime->year, starttime->month,starttime->day, starttime->hour,
                starttime->minute, starttime->second,0x00, 0x00,0x00);	
	return;
}
 

/* The appsink has received a buffer */  
static void new_sample (GstElement *sink, CustomData *data) {  
  GstBuffer *buffer;  

  printf ("#"); 
  /* Retrieve the buffer */  
  g_signal_emit_by_name (sink, "pull-sample", &buffer);  
  if (buffer) {  
    /* The only thing we do in this example is print a * to indicate a received buffer */  
    g_print ("#");  
    gst_buffer_unref (buffer);  
  }  
} 

    
/* This function is called when an error message is posted on the bus */  
static void error_cb (GstBus *bus, GstMessage *msg, CustomData *data) {  
  GError *err;  
  gchar *debug_info;  
    
  /* Print error details on the screen */  
  gst_message_parse_error (msg, &err, &debug_info);  
  g_printerr ("Error received from element %s: %s\n", GST_OBJECT_NAME (msg->src), err->message);  
  g_printerr ("Debugging information: %s\n", debug_info ? debug_info : "none");  
  g_clear_error (&err);  
  g_free (debug_info);  
    
  //g_main_loop_quit (data->main_loop);  
}  


void conv_yuv420_to_mat(cv::Mat &dst, unsigned char* pYUV420, int width, int height)  
{
	using namespace cv;
    if (!pYUV420) {  
        return;  
    }  
  
    IplImage *yuvimage,*rgbimg,*yimg,*uimg,*vimg,*uuimg,*vvimg;  
  
    int nWidth = width;  
    int nHeight = height;  
    rgbimg = cvCreateImage(cvSize(nWidth, nHeight),IPL_DEPTH_8U,3);  
    yuvimage = cvCreateImage(cvSize(nWidth, nHeight),IPL_DEPTH_8U,3);  
  
    yimg = cvCreateImageHeader(cvSize(nWidth, nHeight),IPL_DEPTH_8U,1);  
    uimg = cvCreateImageHeader(cvSize(nWidth/2, nHeight/2),IPL_DEPTH_8U,1);  
    vimg = cvCreateImageHeader(cvSize(nWidth/2, nHeight/2),IPL_DEPTH_8U,1);  
  
    uuimg = cvCreateImage(cvSize(nWidth, nHeight),IPL_DEPTH_8U,1);  
    vvimg = cvCreateImage(cvSize(nWidth, nHeight),IPL_DEPTH_8U,1);  
  
    cvSetData(yimg,pYUV420, nWidth);  
    cvSetData(uimg,pYUV420+nWidth*nHeight, nWidth/2);  
    cvSetData(vimg,pYUV420+long(nWidth*nHeight*1.25), nWidth/2);  
    cvResize(uimg,uuimg,CV_INTER_LINEAR);  
    cvResize(vimg,vvimg,CV_INTER_LINEAR);  
  
    cvMerge(yimg,uuimg,vvimg,NULL,yuvimage);  
    cvCvtColor(yuvimage,rgbimg,CV_YCrCb2RGB);  
  
    cvReleaseImage(&uuimg);  
    cvReleaseImage(&vvimg);  
    cvReleaseImageHeader(&yimg);  
    cvReleaseImageHeader(&uimg);  
    cvReleaseImageHeader(&vimg);  
  
    cvReleaseImage(&yuvimage);  
  
    dst = Mat(rgbimg, true);  
    cvReleaseImage(&rgbimg);  
} 


GstFlowReturn app_sink_new_sample(GstAppSink *sink, gpointer user_data){
	//prog_data* pd = (prog_data*)user_data;
	//printf ("#");
	GstCaps *caps;
	GstStructure *s;
	gint width, height;

	GstSample* sample = gst_app_sink_pull_sample(sink);

	if(sample == NULL) {
	return GST_FLOW_ERROR;
	}
#if 1

	GstBuffer* buffer = gst_sample_get_buffer(sample);

	GstMemory* memory = gst_buffer_get_all_memory(buffer);
	GstMapInfo map_info;

	if(! gst_memory_map(memory, &map_info, GST_MAP_READ)) {
	gst_memory_unref(memory);
	gst_sample_unref(sample);
	return GST_FLOW_ERROR;
	}

	//render using map_info.data
	caps = gst_sample_get_caps (sample);
	s = gst_caps_get_structure (caps, 0);
	/* we need to get the final caps on the buffer to get the size */
	gst_structure_get_int (s, "width", &width);
	gst_structure_get_int (s, "height", &height);
	#if 0
	printf("%dx%d:%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",width,height
		,map_info.data[0],map_info.data[1],map_info.data[2],map_info.data[3],map_info.data[4]
		,map_info.data[5],map_info.data[6],map_info.data[7],map_info.data[8],map_info.data[9]);
	#endif
	
	#if 0
	static int i=0;
	char name[16];
	//snprintf(name,16,"data%d.yuv",i++);
	snprintf(name,16,"data.yuv");
	int out = open(name, O_WRONLY|O_CREAT|O_APPEND,0777);  
    if (-1 == out) // 创建文件失败,则异常返回  
    {
    	printf("open failed\n");
        return GST_FLOW_ERROR;   
    }     
    write(out, map_info.data, map_info.size);     
    close(out);
	#else
	// convert gstreamer data to OpenCV Mat, you could actually
	// resolve height / width from caps...
	cv::Mat frame(cv::Size(width, height+height/2), CV_8UC1, (char*)map_info.data);
	cv::Mat rgbImg;   
	cv::cvtColor(frame, rgbImg, CV_YUV2BGR_NV12);
	//cv::imshow("show", frame);

	//conv_yuv420_to_mat(rgbImg,map_info.data,width, height);
	cv::imshow("rgb show", rgbImg);
	cv::waitKey(30);
	// TODO: synchronize this....
	//frameQueue.push_back(frame);


	#endif


	

	gst_memory_unmap(memory, &map_info);
	gst_memory_unref(memory);
	gst_sample_unref(sample);
	#endif

	return GST_FLOW_OK;
}


int main(int argc, char *argv[])
{
	GstMessage *msg;  
	CustomData data;
	GstBus *bus;
	GstCaps *source_caps;
	GstBuffer *buffer;
	datetime_t StartTime;
	file_info_t fileinfo;
	char startname[512];
	
	GstPadTemplate *tee_src_pad_template;  
    GstPad *tee_app_pad, *tee_video_pad;  
    GstPad *queue_app_pad, *queue_video_pad;
	g_print ("hello\n");
  
	gst_init(&argc, &argv);
	/* Create the elements */
	data.pipeline = gst_pipeline_new("uvc-camera");
	
	data.camera_source = gst_element_factory_make ("v4l2src", "camera_input");
	g_object_set(G_OBJECT(data.camera_source),"device","/dev/video0",NULL);
	data.tee = gst_element_factory_make ("tee", "src_tee");
	
	
	data.app_queue = gst_element_factory_make ("queue", "app_queue");
	data.store_queue = gst_element_factory_make ("queue", "store_queue");
	
	data.app_convert = gst_element_factory_make ("videoconvert", "show_converter");
	data.store_convert = gst_element_factory_make ("videoconvert", "video_converter");
	data.app_sink = gst_element_factory_make ("appsink", "app_sink");//("autovideosink", "show-screen");
	data.store_sink = gst_element_factory_make ("filesink", "file_storage");
	data.encoder = gst_element_factory_make("mpph264enc", "video_encoder");
	data.mux = gst_element_factory_make("mpegtsmux", "video_mux");

	source_caps = gst_caps_new_simple ("video/x-raw","format", G_TYPE_STRING, "NV12",
		"width", G_TYPE_INT, 640,"height", G_TYPE_INT, 480,NULL);
	data.store_capsfilter = gst_element_factory_make("capsfilter", "source_capsfilter");	
	g_object_set(G_OBJECT(data.store_capsfilter),"caps", source_caps, NULL);	
	data.app_capsfilter = gst_element_factory_make("capsfilter", "show_capsfilter");
	g_object_set(G_OBJECT(data.app_capsfilter),"caps", source_caps, NULL);



	/* Configure appsink */  
	g_object_set (data.app_sink, "emit-signals", TRUE, "caps", source_caps, NULL);
	#if 0
	g_signal_connect (data.app_sink, "new-preroll", G_CALLBACK (new_preroll), &data);
	g_signal_connect (data.app_sink, "new-sample", G_CALLBACK (new_sample), &data);  
	//gst_caps_unref (audio_caps);  
	//g_free (audio_caps_text);  
	#else
	GstAppSinkCallbacks* appsink_callbacks = (GstAppSinkCallbacks*)malloc(sizeof(GstAppSinkCallbacks));
	appsink_callbacks->eos = NULL;
	appsink_callbacks->new_preroll = NULL;
	appsink_callbacks->new_sample = app_sink_new_sample;
	gst_app_sink_set_callbacks(GST_APP_SINK(data.app_sink), appsink_callbacks, &data, NULL);//free);
	#endif
	
	if (!data.pipeline || !data.tee || !data.camera_source || !data.store_capsfilter || !data.store_queue || !data.app_capsfilter ||
	!data.store_convert || !data.encoder || !data.mux || !data.store_sink || !data.app_queue||!data.app_convert||!data.app_sink)
	{
		g_printerr("One element could not be created.Exiting.\n");
		return -1;
	}
	gst_bin_add_many(GST_BIN(data.pipeline), data.camera_source, data.store_capsfilter,data.store_queue,data.app_sink,
		data.store_convert, data.encoder, data.mux, data.store_sink,data.app_queue,data.tee,data.app_convert,data.app_capsfilter, NULL);
	if(gst_element_link_many(data.camera_source,data.tee,NULL)!=TRUE )
	{
		printf("101010101010101\n");
	}
	gst_element_link_many(data.store_queue,data.store_capsfilter,data.store_convert, data.encoder, data.mux, data.store_sink, NULL);
	//gst_element_link_many(data.app_queue,data.app_capsfilter,data.app_convert,data.app_sink, NULL);
	gst_element_link_many(data.app_queue,data.app_sink, NULL);
	
	tee_src_pad_template = gst_element_class_get_pad_template(GST_ELEMENT_GET_CLASS(data.tee), "src_%u");
	if(!tee_src_pad_template)
	{
		printf("77777777777777777\n");
	}
	tee_app_pad = gst_element_request_pad (data.tee, tee_src_pad_template, NULL, NULL);  
	if(!tee_app_pad)
	{
		printf("888888888888\n");
	}
	queue_app_pad = gst_element_get_static_pad (data.store_queue, "sink");
	tee_video_pad = gst_element_request_pad (data.tee, tee_src_pad_template, NULL, NULL);
	if(!tee_video_pad)
	{
		printf("999999999999999\n");
	}
	queue_video_pad = gst_element_get_static_pad (data.app_queue, "sink");
	if (gst_pad_link(tee_app_pad, queue_app_pad) != GST_PAD_LINK_OK || gst_pad_link(tee_video_pad, queue_video_pad) != GST_PAD_LINK_OK)
	{  
	  g_printerr ("Tee could not be linked.\n");  
	  gst_object_unref (data.pipeline);  
	  return -1;  
	}    
	
	memset(&fileinfo,0,sizeof(fileinfo));
	
	GetSysTime(&fileinfo.starttime);
	getfilename(&fileinfo.starttime,startname);
	//g_object_set(G_OBJECT(data.store_sink),"location", startname, NULL);
	g_object_set(G_OBJECT(data.store_sink),"location", "save/store.h264", NULL);
	//g_object_set(G_OBJECT(data.app_sink),"location", "save/app.ts", NULL);
	bus = gst_pipeline_get_bus(GST_PIPELINE(data.pipeline));
	data.terminate = FALSE;
	gst_element_set_state(data.pipeline, GST_STATE_PLAYING);

	//g_signal_connect (G_OBJECT (bus), "message::error", (GCallback)error_cb, &data);  
	
	while (!data.terminate)
	{
		#if 0
		// TODO: synchronize...
		if (frameQueue.size() > 0) {
			// this lags pretty badly even when grabbing frames from webcam
			cv::Mat frame = frameQueue.front();
			cv::Mat edges;
			//cv::cvtColor(frame, edges, CV_RGB2GRAY);
			//cv::GaussianBlur(edges, edges, cv::Size(7,7), 1.5, 1.5);
			//cv::Canny(edges, edges, 0, 30, 3);
			cv::imshow("edges", frame);
			cv::waitKey(30);
			frameQueue.clear();
		}
		#endif
	
		msg = gst_bus_timed_pop_filtered (bus, 1100 * GST_MSECOND,(GstMessageType)(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));

		if (msg != NULL)
		{
			handle_message (&data, msg);
		}
		else
		{
			GetSysTime(&fileinfo.endtime);
			if(CompearTime(&fileinfo.endtime,&fileinfo.starttime)>=2000)
			{
				data.terminate = TRUE;
				gst_element_set_state(data.pipeline,GST_STATE_NULL);


				#if 0
				getfilename(&fileinfo.endtime,startname);
				gst_element_set_state(data.pipeline,GST_STATE_NULL);
				g_object_set(G_OBJECT(data.store_sink),"location", startname, NULL);
				gst_element_set_state(data.pipeline, GST_STATE_PLAYING);
//				rename(fileinfo.filestartname, fileinfo.fileendname);
				GetSysTime(&fileinfo.starttime);
				#endif
			}
		}
	} 
	
	gst_element_release_request_pad (data.tee, tee_app_pad);
	gst_element_release_request_pad (data.tee, tee_video_pad);
	gst_object_unref (tee_app_pad);  
	gst_object_unref (tee_video_pad); 
	gst_object_unref (queue_app_pad);  
	gst_object_unref (queue_video_pad);

	gst_object_unref(bus);
	gst_element_set_state(data.pipeline, GST_STATE_NULL);
	gst_object_unref(GST_OBJECT(data.pipeline));
	return 0;
}



