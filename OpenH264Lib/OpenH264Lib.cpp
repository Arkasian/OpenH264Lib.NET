// ����� ���C�� DLL �t�@�C���ł��B

#include "stdafx.h"

#include "OpenH264Lib.h"

namespace OpenH264Lib {

	// Reference
	// https://github.com/kazuki/video-codec.js
	// file://openh264-master\test\api\BaseEncoderTest.cpp
	// file://openh264-master\codec\console\enc\src\welsenc.cpp

	// C#����byte[]�Ƃ��ČĂяo���\
	int OpenH264Encoder::Encode(array<Byte> ^data, float timestamp)
	{
		// http://xptn.dtiblog.com/blog-entry-21.html
		pin_ptr<Byte> ptr = &data[0];
		int rc = Encode(ptr, timestamp);
		ptr = nullptr; // unpin
		return rc;
	}

	// C#�����unsafe&fixed�𗘗p���Ȃ��ƌĂяo���ł��Ȃ�
	int OpenH264Encoder::Encode(unsigned char *data, float timestamp)
	{
		memcpy(i420_buffer, data, buffer_size);

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
}
