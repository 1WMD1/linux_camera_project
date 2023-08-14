#include <config.h>
#include <disp_manager.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <string.h>



// 在函数声明前加上static关键字，表示该函数具有静态的存储类别。
// 静态函数只能在当前源文件中使用，不能被其他源文件调用。这样做的目的是限制函数的作用域;
// 使其只能在当前源文件中访问，避免函数被其他源文件意外调用或修改。
static int FBDeviceInit(void);//"framebuffer显示设备"的初始化函数
static int FBShowPixel(int iX, int iY, unsigned int dwColor);//设置FrameBuffer的指定象素为某颜色
static int FBCleanScreen(unsigned int dwBackColor);// "framebuffer显示设备"的清屏函数
static int FBShowPage(PT_PixelDatas ptPixelDatas);// 把PT_VideoMem中的颜色数据在FrameBuffer上显示出来


static T_DispOpr g_tFBOpr = {
	.name        = "framebuffer",
	.DeviceInit  = FBDeviceInit,
	.ShowPixel   = FBShowPixel,
	.CleanScreen = FBCleanScreen,
	.ShowPage    = FBShowPage,
};


int FBInit(void)
{
	return RegisterDispOpr(&g_tFBOpr);
}


static int g_fd;
static struct fb_var_screeninfo g_tFBVar;//这个数据结构由驱动程序定义
static struct fb_fix_screeninfo g_tFBFix;//驱动数据结构			
static unsigned int g_dwScreenSize;
static unsigned char *g_pucFBMem;

static unsigned int g_dwLineWidth;
static unsigned int g_dwPixelWidth;


//"framebuffer显示设备"的初始化函数，打开屏幕读取参数，放到两个BUFFER中，并构建了一个链表结构的结构体，并赋初值
static int FBDeviceInit(void)
{
	int ret;
	//打开设备节点"/dev/fb0"
	g_fd = open(FB_DEVICE_NAME,O_RDWR);
	if (0 > g_fd)
	{
		DBG_PRINTF("can't open %s\n", FB_DEVICE_NAME);
	}
	//发送控制请求，由驱动程序定义
	ret = ioctl(g_fd,FBIOGET_VSCREENINFO,&g_tFBVar);//传出参数
	if (ret < 0)
	{
		DBG_PRINTF("can't get fb's var\n");
		return -1;
	}
	//发送控制请求，由驱动程序定义
	ret = ioctl(g_fd, FBIOGET_FSCREENINFO, &g_tFBFix);//传出参数
	if (ret < 0)
	{
		DBG_PRINTF("can't get fb's fix\n");
		return -1;
	}
	//获得屏幕尺寸，并且mmap开辟虚拟内存映射空间，增快传输速度
	g_dwScreenSize = g_tFBVar.xres * g_tFBVar.yres * g_tFBVar.bits_per_pixel / 8;
	g_pucFBMem = (unsigned char *)mmap(NULL , g_dwScreenSize, PROT_READ | PROT_WRITE, MAP_SHARED, g_fd, 0);//调用驱动mmap的函数，来实现虚拟内存映射

// mmap()是一个系统调用函数，用于将文件或设备的一块内存映射到进程的虚拟地址空间中。它的原型如下：
// void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset);

// 参数说明：
// addr：指定映射的起始地址，通常设为NULL，由系统自动分配。
// length：映射的长度，以字节为单位。
// prot：映射区域的保护方式，指定内存区域的可读、可写、可执行等属性。
// flags：指定映射区域的一些特性，如映射区域是否可以共享、是否可以修改等。
// fd：要映射的文件描述符，如果映射的是文件，则该参数为文件描述符；如果映射的是设备，则该参数为设备的文件描述符。
// offset：映射的起始位置，对于文件映射，通常为0；对于设备映射，通常为设备的偏移量。
// 返回值：

// 成功时，返回映射区域的起始地址；失败时，返回MAP_FAILED(-1)。
// mmap()函数的作用是将一个文件或设备的内容映射到进程的地址空间中，使得进程可以直接访问该文件或设备的数据。通过对映射区域进行读写操作，可以实现对文件或设备的直接操作，而无需使用read()和write()等系统调用。
// 需要注意的是，使用mmap()函数映射的内存区域必须按照页对齐，即起始地址和长度必须是页大小的整数倍。另外，对映射区域的读写操作可能会触发页错误，需要使用mprotect()函数设置合适的保护属性。
// 使用mmap()函数时，需要注意对返回的映射地址的合法性和正确性进行判断和处理，以及在不再需要映射区域时使用munmap()函数释放映射的内存。



	if (0 > g_pucFBMem)	
	{
		DBG_PRINTF("can't mmap\n");
		return -1;
	}


	g_tFBOpr.iXres       = g_tFBVar.xres;
	g_tFBOpr.iYres       = g_tFBVar.yres;
	g_tFBOpr.iBpp        = g_tFBVar.bits_per_pixel;
	g_tFBOpr.iLineWidth  = g_tFBVar.xres * g_tFBOpr.iBpp / 8;
	g_tFBOpr.pucDispMem  = g_pucFBMem;
	
	g_dwLineWidth  = g_tFBVar.xres * g_tFBVar.bits_per_pixel / 8;
	g_dwPixelWidth = g_tFBVar.bits_per_pixel / 8;

 	return 0;
}

//设置FrameBuffer的指定象素为某颜色
static int FBShowPixel(int iX, int iY, unsigned int dwColor)
{

	int iRed;
	int iGreen;
	int iBlue;

	if ((iX >= g_tFBVar.xres) || (iY >= g_tFBVar.yres))
	{
		DBG_PRINTF("out of region\n");
		return -1;
	}
	unsigned char *pucFB;
	unsigned short *pwFB16bpp;
	unsigned int *pdwFB32bpp;
	unsigned short wColor16bpp; /* 565 */
	pucFB = g_pucFBMem + g_dwLineWidth * iY + g_dwPixelWidth * iX;
	pwFB16bpp  = (unsigned short *)pucFB;
	pdwFB32bpp = (unsigned int *)pucFB;

	switch (g_tFBVar.bits_per_pixel)//一个像素多少位
	{
		case 8:
		{
			*pucFB = (unsigned char)dwColor;
			break;
		}

		case 16:
		{
			/* 从dwBackColor中取出红绿蓝三原色,
			 * 构造为16Bpp的颜色
			 */
			iRed   = (dwColor >> (16+3)) & 0x1f;
			iGreen = (dwColor >> (8+2)) & 0x3f;
			iBlue  = (dwColor >> 3) & 0x1f;
			wColor16bpp = (iRed << 11) | (iGreen << 5) | iBlue;
			*pwFB16bpp	= wColor16bpp;

		}

		case 32；
		{
			*pdwFB32bpp = dwColor;
			break;
		}

		default:
		{
			DBG_PRINTF("can't support %d bpp\n", g_tFBVar.bits_per_pixel);
			return -1;
		}

	}
	return 0;

}

// "framebuffer显示设备"的清屏函数
static int FBCleanScreen(unsigned int dwBackColor)
{
	
	unsigned short *pwFB16bpp;
	unsigned int *pdwFB32bpp;
	unsigned short wColor16bpp; /* 565 */
	int iRed;
	int iGreen;
	int iBlue;
	int i = 0;

	unsigned char *pucFB;
	pucFB      = g_pucFBMem; //虚拟内存映射的地址
	pwFB16bpp  = (unsigned short *)pucFB;
	pdwFB32bpp = (unsigned int *)pucFB;

	switch (g_tFBVar.bits_per_pixel)
	{
		case 8:
		{
			memset(g_pucFBMem, dwBackColor, g_dwScreenSize);
			break;
// memset函数是C语言中的一个库函数，用于将一块内存区域的内容设置为指定的值。
// 函数原型如下：
// void *memset(void *s, int c, size_t n);

// 参数说明：
// s：指向要设置的内存区域的指针。
// c：要设置的值，通常是一个整数。
// n：要设置的字节数。
// 函数功能：
// memset函数将指定的值c复制到s所指向的内存区域的前n个字节中。
// 使用示例：

		}

		case 16：
		{
			/* 从dwBackColor中取出红绿蓝三原色,
			 * 构造为16Bpp的颜色
			 */
			iRed   = (dwBackColor >> (16+3)) & 0x1f;
			iGreen = (dwBackColor >> (8+2)) & 0x3f;
			iBlue  = (dwBackColor >> 3) & 0x1f;
			wColor16bpp = (iRed << 11) | (iGreen << 5) | iBlue;
			while (i < g_dwScreenSize)
			{
				*pwFB16bpp	= wColor16bpp;
				pwFB16bpp++;
				i += 2;
			}
			break;

		}

		case 32:
		{
			while (i < g_dwScreenSize)
			{
				*pdwFB32bpp	= dwBackColor;
				pdwFB32bpp++;
				i += 4;
			}
			break;
		}

		default:
		{
			DBG_PRINTF("can't support %d bpp\n", g_tFBVar.bits_per_pixel);
			return -1;
		}
	}

	return 0;

}




// /* 图片的象素数据 */
// typedef struct PixelDatas {
// 	int iWidth;   /* 宽度: 一行有多少个象素 */
// 	int iHeight;  /* 高度: 一列有多少个象素 */
// 	int iBpp;     /* 一个象素用多少位来表示 */
// 	int iLineBytes;  /* 一行数据有多少字节 */
// 	int iTotalBytes; /* 所有字节数 */ 
// 	unsigned char *aucPixelDatas;  /* 象素数据存储的地方 */
// }T_PixelDatas, *PT_PixelDatas;

// 把PT_VideoMem中的颜色数据在FrameBuffer上显示出来
static int FBShowPage(PT_PixelDatas ptPixelDatas)
{
	if (g_tFBOpr.pucDispMem != ptPixelDatas->aucPixelDatas)
    {
    	//把图片的源数据赋值到共享虚拟内存映射中去
    	memcpy(g_tFBOpr.pucDispMem, ptPixelDatas->aucPixelDatas, ptPixelDatas->iTotalBytes);
// memcpy是C语言中的一个库函数，用于将一块内存区域的内容复制到另一块内存区域。
// 函数原型如下：
// void *memcpy(void *dest, const void *src, size_t n);

// 参数说明：
// dest：目标内存区域的起始地址，即要将数据复制到的位置。
// src：源内存区域的起始地址，即要复制的数据的位置。
// n：要复制的字节数。

// 函数功能：
// memcpy函数将src所指向的内存区域的前n个字节复制到dest所指向的内存区域中。如果源和目标区域有重叠，复制行为是未定义的。
   }
   return 0;
}
