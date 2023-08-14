#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <config.h>
#include <disp_manager.h>
#include <video_manager.h>
#include <convert_manager.h>
#include <render.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

int main(int argc, char const *argv[])
{
	int iLcdWidth;
    int iLcdHeigt;
    int iLcdBpp;

    int iTopLeftX;
    int iTopLeftY;

    float k;
   	int iError;
    T_VideoDevice tVideoDevice;
    PT_VideoConvert ptVideoConvert;
    int iPixelFormatOfVideo;
    int iPixelFormatOfDisp;


    PT_VideoBuf ptVideoBufCur;
    T_VideoBuf tVideoBuf;
    T_VideoBuf tConvertBuf;
    T_VideoBuf tZoomBuf;
    T_VideoBuf tFrameBuf;
    
    if (argc != 2)
    {
        printf("Usage:\n");
        printf("%s </dev/video0,1,...>\n", argv[0]);
        return -1;
    }
    

/*1.初始化显示屏的相关信息放到构造的结构体链表中*/
	//1.1 注册framebuffer设备显示屏
	DisplayInit();
	//1.2可能可支持多个显示设备: 选择和初始化指定的显示设备:frambuffer 
	SelectAndInitDefaultDispDev("framebuffer");
	//1.3 取出参数显示屏的参数信息
	GetDispResolution(&iLcdWidth, &iLcdHeigt, &iLcdBpp);

	//1.4 取出构造T_VideoBuf结构体：来源于显示屏的参数信息
	GetVideoBufForDisplay(&tFrameBuf);
	//1.5获得显示屏的像素
	iPixelFormatOfDisp = tFrameBuf.iPixelFormat;



/*2. 初始化摄像头相关*/
	//2.1.注册摄像头
	VideoInit();
	
	//2.2.摄像头匹配
    iError = VideoDeviceInit(argv[1], &tVideoDevice); //这边手动传参
    if (iError)
    {
        DBG_PRINTF("VideoDeviceInit for %s error!\n", argv[1]);
        return -1;
    }

    //2.3.获取摄像头参数信息
    int iPixelFormatOfVideo = tVideoDevice.ptOPr->GetFormat(&tVideoDevice);

    
    //2.4.注册支持的视频转换工具
    VideoConvertInit();

    //2.5 输入你前面获得是屏幕与摄像头的像素格式，输入进去选择适合的视屏转换工具
	ptVideoConvert = GetVideoConvertForFormats(iPixelFormatOfVideo, iPixelFormatOfDisp);
    if (NULL == ptVideoConvert)
    {
        DBG_PRINTF("can not support this format convert\n");
        return -1;
    }


/*3.启动摄像头设备*/
    iError = tVideoDevice.ptOPr->StartDevice(&tVideoDevice);
    if (iError)
    {
        DBG_PRINTF("StartDevice for %s error!\n", argv[1]);
        return -1;
    }
	//3.1定义三个BUF,来存取信息，先全部设为0
    memset(&tVideoBuf, 0, sizeof(tVideoBuf));
    memset(&tConvertBuf, 0, sizeof(tConvertBuf));
 	tConvertBuf.iPixelFormat     = iPixelFormatOfDisp;
    tConvertBuf.tPixelDatas.iBpp = iLcdBpp

    memset(&tZoomBuf, 0, sizeof(tZoomBuf));

    while(1)
    {
/*4. 读入摄像头数据 */
        iError = tVideoDevice.ptOPr->GetFrame(&tVideoDevice, &tVideoBuf);
        if (iError)
        {
            DBG_PRINTF("GetFrame for %s error!\n", argv[1]);
            return -1;
        }
        ptVideoBufCur = &tVideoBuf;

        if (iPixelFormatOfVideo != iPixelFormatOfDisp)
        {
    //4.1 转换为RGB 
            iError = ptVideoConvert->Convert(&tVideoBuf, &tConvertBuf);
            DBG_PRINTF("Convert %s, ret = %d\n", ptVideoConvert->name, iError);
            if (iError)
            {
                DBG_PRINTF("Convert for %s error!\n", argv[1]);
                return -1;
            }            
            ptVideoBufCur = &tConvertBuf;
        }



    // 4.2 如果图像分辨率大于LCD, 缩放 
        if (ptVideoBufCur->tPixelDatas.iWidth > iLcdWidth || ptVideoBufCur->tPixelDatas.iHeight > iLcdHeigt)
        {
            /* 确定缩放后的分辨率 */
            /* 把图片按比例缩放到VideoMem上, 居中显示
             * 1. 先算出缩放后的大小
             */
            k = (float)ptVideoBufCur->tPixelDatas.iHeight / ptVideoBufCur->tPixelDatas.iWidth;
            tZoomBuf.tPixelDatas.iWidth  = iLcdWidth;
            tZoomBuf.tPixelDatas.iHeight = iLcdWidth * k;
            if ( tZoomBuf.tPixelDatas.iHeight > iLcdHeigt)
            {
                tZoomBuf.tPixelDatas.iWidth  = iLcdHeigt / k;
                tZoomBuf.tPixelDatas.iHeight = iLcdHeigt;
            }
            tZoomBuf.tPixelDatas.iBpp        = iLcdBpp;
            tZoomBuf.tPixelDatas.iLineBytes  = tZoomBuf.tPixelDatas.iWidth * tZoomBuf.tPixelDatas.iBpp / 8;
            tZoomBuf.tPixelDatas.iTotalBytes = tZoomBuf.tPixelDatas.iLineBytes * tZoomBuf.tPixelDatas.iHeight;

            if (!tZoomBuf.tPixelDatas.aucPixelDatas)
            {
                tZoomBuf.tPixelDatas.aucPixelDatas = malloc(tZoomBuf.tPixelDatas.iTotalBytes);
            }
            
            PicZoom(&ptVideoBufCur->tPixelDatas, &tZoomBuf.tPixelDatas);
            ptVideoBufCur = &tZoomBuf;
        }

     // 4.3合并进framebuffer 
        /* 接着算出居中显示时左上角坐标 */
        iTopLeftX = (iLcdWidth - ptVideoBufCur->tPixelDatas.iWidth) / 2;
        iTopLeftY = (iLcdHeigt - ptVideoBufCur->tPixelDatas.iHeight) / 2;

        PicMerge(iTopLeftX, iTopLeftY, &ptVideoBufCur->tPixelDatas, &tFrameBuf.tPixelDatas);

        FlushPixelDatasToDev(&tFrameBuf.tPixelDatas);

        iError = tVideoDevice.ptOPr->PutFrame(&tVideoDevice, &tVideoBuf);
        if (iError)
        {
            DBG_PRINTF("PutFrame for %s error!\n", argv[1]);
            return -1;
        }                    

        /* 把framebuffer的数据刷到LCD上, 显示 */


    }

	return 0;
}