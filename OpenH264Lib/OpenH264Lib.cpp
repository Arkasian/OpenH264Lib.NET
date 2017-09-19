// ����� ���C�� DLL �t�@�C���ł��B

#include "stdafx.h"

#include "OpenH264Lib.h"

using namespace System::Drawing;
using namespace System::Drawing::Imaging;

namespace OpenH264Lib {

	// Reference
	// https://github.com/kazuki/video-codec.js
	// file://openh264-master\test\api\BaseEncoderTest.cpp
	// file://openh264-master\codec\console\enc\src\welsenc.cpp

	int OpenH264Encoder::Encode(Bitmap^ bmp, float timestamp)
	{
		unsigned char* rgba = BitmapToRGBA(bmp, bmp->Width, bmp->Height);
		unsigned char* i420 = RGBAtoYUV420Planar(rgba, bmp->Width, bmp->Height);
		int rc = Encode(i420, timestamp);
		delete rgba;
		delete i420;
		return rc;
	}

	// C#����byte[]�Ƃ��ČĂяo���\
	int OpenH264Encoder::Encode(array<Byte> ^i420, float timestamp)
	{
		// http://xptn.dtiblog.com/blog-entry-21.html
		pin_ptr<Byte> ptr = &i420[0];
		int rc = Encode(ptr, timestamp);
		ptr = nullptr; // unpin
		return rc;
	}

	// C#�����unsafe&fixed�𗘗p���Ȃ��ƌĂяo���ł��Ȃ�
	int OpenH264Encoder::Encode(unsigned char *i420, float timestamp)
	{
		memcpy(i420_buffer, i420, buffer_size);

		// ���Ԋu�ŃL�[�t���[��(I�t���[��)��}��
		if (num_of_frames++ % keyframe_interval == 0) {
			encoder->ForceIntraFrame(true);
		}

		// �^�C���X�^���v��ms�ŕt��
		pic->uiTimeStamp = (long long)(timestamp * 1000.0f);

		// �t���[���G���R�[�h
		int ret = encoder->EncodeFrame(pic, bsi);
		if (ret != 0) {
			return ret;
		}

		// �G���R�[�h�����R�[���o�b�N
		if (bsi->eFrameType != videoFrameTypeSkip) {
			OnEncode(dynamic_cast<SFrameBSInfo%>(*bsi));
		}

		return 0;
	}

	void OpenH264Encoder::OnEncode(const SFrameBSInfo% info)
	{
		for (int i = 0; i < info.iLayerNum; ++i) {
			const SLayerBSInfo& layerInfo = info.sLayerInfo[i];
			int layerSize = 0;
			for (int j = 0; j < layerInfo.iNalCount; ++j) {
				layerSize += layerInfo.pNalLengthInByte[j];
			}

			bool keyFrame = (info.eFrameType == videoFrameTypeIDR) || (info.eFrameType == videoFrameTypeI);
			OnEncodeFunc(layerInfo.pBsBuf, layerSize, keyFrame);
		}
	}

	// �G���R�[�_�[�̐ݒ�
	// width, height:�摜�T�C�Y
	// fps:�t���[�����[�g
	// onEncode:1�t���[���G���R�[�h���邲�ƂɌĂяo�����R�[���o�b�N
	int OpenH264Encoder::Setup(int width, int height, float fps, void *onEncode)
	{
		OnEncodeFunc = static_cast<OnEncodeFuncPtr>(onEncode);

		// ���t���[�����ƂɃL�[�t���[��(I�t���[��)��}�����邩
		// �ʏ�̓���(30fps)�ł�60(�܂�2�b����)�ʂ��K�؂炵���B
		keyframe_interval = (int)(fps * 2);

		// encoder�̏������Bencoder->Initialize���g���B
		// encoder->InitializeExt�͏������ɕK�v�ȃp�����[�^����������
		SEncParamBase base;
		memset(&base, 0, sizeof(SEncParamBase));
		base.iPicWidth = width;
		base.iPicHeight = height;
		base.fMaxFrameRate = fps;
		base.iUsageType = CAMERA_VIDEO_REAL_TIME;
		base.iTargetBitrate = 5000000;
		int rc = encoder->Initialize(&base);
		if (rc != 0) return -1;

		// �\�[�X�t���[���������m��
		buffer_size = width * height * 3 / 2;
		i420_buffer = new unsigned char[buffer_size];
		pic = new SSourcePicture();
		pic->iPicWidth = width;
		pic->iPicHeight = height;
		pic->iColorFormat = videoFormatI420;
		pic->iStride[0] = pic->iPicWidth;
		pic->iStride[1] = pic->iStride[2] = pic->iPicWidth >> 1;
		pic->pData[0] = i420_buffer;
		pic->pData[1] = pic->pData[0] + width * height;
		pic->pData[2] = pic->pData[1] + (width * height >> 2);

		// �r�b�g�X�g���[���m��
		bsi = new SFrameBSInfo();

		return 0;
	};

	// �R���X�g���N�^
	OpenH264Encoder::OpenH264Encoder()
	{
		HMODULE hDll = LoadLibrary(L"openh264-1.7.0-win32.dll");
		if (hDll == NULL) {
			throw gcnew System::DllNotFoundException("Unable to load 'openh264-1.7.0-win32.dll'");
		}

		typedef int(__stdcall *WelsCreateSVCEncoderFunc)(ISVCEncoder** ppEncoder);
		WelsCreateSVCEncoderFunc createEncoder = (WelsCreateSVCEncoderFunc)GetProcAddress(hDll, "WelsCreateSVCEncoder");
		if (createEncoder == NULL) {
			throw gcnew System::DllNotFoundException("Unable to load WelsCreateSVCEncoder func in 'openh264-1.7.0-win32.dll'");
		}

		ISVCEncoder* enc = nullptr;
		int rc = createEncoder(&enc);
		encoder = enc;
	}

	// �f�X�g���N�^�F���\�[�X��ϋɓI�ɉ������ׂɂ��郁�\�b�h�BC#��Dispose�ɑΉ��B
	// �}�l�[�W�h�A�A���}�l�[�W�h�����Ƃ��������B
	OpenH264Encoder::~OpenH264Encoder()
	{
		// �}�l�[�W�h������Ȃ�
		// �t�@�C�i���C�U�Ăяo��
		this->!OpenH264Encoder();
	}

	// �t�@�C�i���C�U�F���\�[�X�̉�����Y��ɂ���Q���ŏ����ɗ}����ׂɂ��郁�\�b�h�B
	// �A���}�l�[�W�h���\�[�X���������B
	OpenH264Encoder::!OpenH264Encoder()
	{
		// �A���}�l�[�W�h���
		HMODULE hDll = LoadLibrary(L"openh264-1.7.0-win32.dll");
		if (hDll == NULL) {
			throw gcnew System::DllNotFoundException("Unable to load 'openh264-1.7.0-win32.dll'");
		}

		typedef void(__stdcall *WelsDestroySVCEncoderFunc)(ISVCEncoder* ppEncoder);
		WelsDestroySVCEncoderFunc destroyEncoder = (WelsDestroySVCEncoderFunc)GetProcAddress(hDll, "WelsDestroySVCEncoder");
		if (destroyEncoder == NULL) {
			throw gcnew System::DllNotFoundException("Unable to load WelsDestroySVCEncoder func in 'openh264-1.7.0-win32.dll'");
		}

		encoder->Uninitialize();
		destroyEncoder(encoder);

		delete i420_buffer;
		delete pic;
		delete bsi;
	}

	unsigned char* OpenH264Encoder::BitmapToRGBA(Bitmap^ bmp, int width, int height)
	{
		//1�s�N�Z��������̃o�C�g�����擾����
		int pixelSize = 0;
		switch (bmp->PixelFormat)
		{
		case System::Drawing::Imaging::PixelFormat::Format24bppRgb:
			pixelSize = 3;
			break;
		case System::Drawing::Imaging::PixelFormat::Format32bppArgb:
		case System::Drawing::Imaging::PixelFormat::Format32bppPArgb:
		case System::Drawing::Imaging::PixelFormat::Format32bppRgb:
			pixelSize = 4;
			break;
		default:
			throw gcnew ArgumentException("1�s�N�Z��������24�܂���32�r�b�g�̌`���̃C���[�W�̂ݗL���ł��B");
		}

		BitmapData^ bmpDate = bmp->LockBits(System::Drawing::Rectangle(0, 0, width, height), ImageLockMode::ReadOnly, bmp->PixelFormat);
		byte *ptr = (byte *)bmpDate->Scan0.ToPointer();

		unsigned char* rgba_buffer = new unsigned char[width * height * 4];

		int cnt = 0;
		for (int y = 0; y <= height - 1; y++)
		{
			for (int x = 0; x <= width - 1; x++)
			{
				//�s�N�Z���f�[�^�ł̃s�N�Z��(x,y)�̊J�n�ʒu���v�Z����
				int pos = y * bmpDate->Stride + x * pixelSize;

				rgba_buffer[cnt + 0] = ptr[pos + 0]; // r
				rgba_buffer[cnt + 1] = ptr[pos + 1]; // g
				rgba_buffer[cnt + 2] = ptr[pos + 2]; // b
				//rgba_buffer[cnt + 3] = 0x00; unused
				cnt += 4;
			}
		}

		//���b�N����������
		bmp->UnlockBits(bmpDate);

		return rgba_buffer;
	}

	// http://qiita.com/gomachan7/items/54d43693f943a0986e95
	unsigned char* OpenH264Encoder::RGBAtoYUV420Planar(unsigned char *rgba, int width, int height)
	{
		int frameSize = width * height;
		int yIndex = 0;
		int uIndex = frameSize;
		int vIndex = frameSize + (frameSize / 4);
		int r, g, b, y, u, v;
		int index = 0;
		unsigned char* yuv420p = new unsigned char[width * height * 3 / 2];

		for (int j = 0; j < height; j++)
		{
			for (int i = 0; i < width; i++)
			{
				r = rgba[index * 4 + 0] & 0xff;
				g = rgba[index * 4 + 1] & 0xff;
				b = rgba[index * 4 + 2] & 0xff;
				// a = rgba[index * 4 + 3] & 0xff; unused

				y = (int)(0.257 * r + 0.504 * g + 0.098 * b) + 16;
				u = (int)(0.439 * r - 0.368 * g - 0.071 * b) + 128;
				v = (int)(-0.148 * r - 0.291 * g + 0.439 * b) + 128;

				yuv420p[yIndex++] = (byte)((y < 0) ? 0 : ((y > 255) ? 255 : y));

				if (j % 2 == 0 && index % 2 == 0)
				{
					yuv420p[uIndex++] = (byte)((u < 0) ? 0 : ((u > 255) ? 255 : u));
					yuv420p[vIndex++] = (byte)((v < 0) ? 0 : ((v > 255) ? 255 : v));
				}

				index++;
			}
		}

		return yuv420p;
	}
}
