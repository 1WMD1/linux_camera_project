
#ifndef _RENDER_H
#define _RENDER_H

#include <pic_operation.h>
#include <disp_manager.h>


 // * 函数名称： PicZoom
 // * 功能描述： 近邻取样插值方法缩放图片
 // *            注意该函数会分配内存来存放缩放后的图片,用完后要用free函数释放掉
 // *            "近邻取样插值"的原理请参考网友"lantianyu520"所著的"图像缩放算法"
 // * 输入参数： ptOriginPic - 内含原始图片的象素数据
 // *            ptBigPic    - 内含缩放后的图片的象素数据

int PicZoom(PT_PixelDatas ptOriginPic, PT_PixelDatas ptZoomPic);

 // * 函数名称： PicMerge
 // * 功能描述： 把小图片合并入大图片里
 // * 输入参数： iX,iY      - 小图片合并入大图片的某个区域, iX/iY确定这个区域的左上角座标
 // *            ptSmallPic - 内含小图片的象素数据
 // *            ptBigPic   - 内含大图片的象素数据
 // * 输出参数： 无

int PicMerge(int iX, int iY, PT_PixelDatas ptSmallPic, PT_PixelDatas ptBigPic);


#endif /* _RENDER_H */

