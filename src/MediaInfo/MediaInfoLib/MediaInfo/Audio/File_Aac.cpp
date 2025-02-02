// File_Aac - Info for AAC (Raw) files
// Copyright (C) 2008-2012 MediaArea.net SARL, Info@MediaArea.net
//
// This library is free software: you can redistribute it and/or modify it
// under the terms of the GNU Library General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// any later version.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU Library General Public License for more details.
//
// You should have received a copy of the GNU Library General Public License
// along with this library. If not, see <http://www.gnu.org/licenses/>.
//
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

//---------------------------------------------------------------------------
// Pre-compilation
#include "MediaInfo/PreComp.h"
#ifdef __BORLANDC__
    #pragma hdrstop
#endif
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include "MediaInfo/Setup.h"
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#if defined(MEDIAINFO_AAC_YES)
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
#include "MediaInfo/Audio/File_Aac.h"
#include "MediaInfo/MediaInfo_Config_MediaInfo.h"
//---------------------------------------------------------------------------

namespace MediaInfoLib
{

//***************************************************************************
// Infos
//***************************************************************************

//***************************************************************************
// Constructor/Destructor
//***************************************************************************

//---------------------------------------------------------------------------
File_Aac::File_Aac()
:File__Analyze(), File__Tags_Helper()
{
    //File__Tags_Helper
    Base=this;

    //Configuration
    MustSynchronize=true;
    Buffer_TotalBytes_FirstSynched_Max=64*1024;
    PTS_DTS_Needed=true;
    IsRawStream=true;

    //In
    Frame_Count_Valid=MediaInfoLib::Config.ParseSpeed_Get()>=0.5?128:(MediaInfoLib::Config.ParseSpeed_Get()>=0.3?32:2);
    FrameIsAlwaysComplete=false;
    Mode=Mode_Unknown;

    audioObjectType=(int8u)-1;
    extensionAudioObjectType=(int8u)-1;
    channelConfiguration=(int8u)-1;
    frame_length=1024;
    sampling_frequency_index=(int8u)-1;
    sampling_frequency=(int32u)-1;
    extension_sampling_frequency_index=(int8u)-1;
    extension_sampling_frequency=(int32u)-1;
    aacSpectralDataResilienceFlag=false;
    aacSectionDataResilienceFlag=false;
    aacScalefactorDataResilienceFlag=false;
    FrameSize_Min=(int64u)-1;
    FrameSize_Max=0;

    //Temp - Main
    muxConfigPresent=true;
    audioMuxVersionA=false;

    //Temp - General Audio
    sbr=NULL;
    ps=NULL;

    //Temp
    CanFill=false;
}

//---------------------------------------------------------------------------
File_Aac::~File_Aac()
{
    delete sbr;
    delete ps;
}

//***************************************************************************
// Streams management
//***************************************************************************

//---------------------------------------------------------------------------
void File_Aac::Streams_Fill()
{
    switch(Mode)
    {
        case Mode_LATM : Fill(Stream_General, 0, General_Format, "LATM"); if (IsSub) Fill(Stream_Audio, 0, Audio_MuxingMode, "LATM"); break;
        default : ;
    }

    for (std::map<std::string, Ztring>::iterator Info=Infos_General.begin(); Info!=Infos_General.end(); ++Info)
        Fill(Stream_General, 0, Info->first.c_str(), Info->second);
    File__Tags_Helper::Stream_Prepare(Stream_Audio);
    for (std::map<std::string, Ztring>::iterator Info=Infos.begin(); Info!=Infos.end(); ++Info)
        Fill(Stream_Audio, StreamPos_Last, Info->first.c_str(), Info->second);

    switch(Mode)
    {
        case Mode_ADTS    : File__Tags_Helper::Streams_Fill(); break;
        default           : ;
    }
}

//---------------------------------------------------------------------------
void File_Aac::Streams_Finish()
{
    switch(Mode)
    {
        case Mode_ADIF    :
        case Mode_ADTS    : File__Tags_Helper::Streams_Finish(); break;
        default           : ;
    }

    if (FrameSize_Min!=(int32u)-1 && FrameSize_Max)
    {
        if (FrameSize_Max>FrameSize_Min*1.02)
        {
            Fill(Stream_Audio, 0, Audio_BitRate_Mode, "VBR", Unlimited, true, true);
            if (Config->ParseSpeed>=1.0)
            {
                Fill(Stream_Audio, 0, Audio_BitRate_Minimum, ((float64)FrameSize_Min)/1024*48000*8, 0);
                Fill(Stream_Audio, 0, Audio_BitRate_Maximum, ((float64)FrameSize_Max)/1024*48000*8, 0);
            }
        }
        else if (Config->ParseSpeed>=1.0)
        {
            Fill(Stream_Audio, 0, Audio_BitRate_Mode, "CBR");
        }
    }
}

//***************************************************************************
// Buffer - File header
//***************************************************************************

//---------------------------------------------------------------------------
bool File_Aac::FileHeader_Begin()
{
    switch(Mode)
    {
        case Mode_AudioSpecificConfig :
        case Mode_ADIF                :
                                        MustSynchronize=false; break;
        default                       : ; //Synchronization is requested, and this is the default
    }

    switch(Mode)
    {
        case Mode_Unknown             :
        case Mode_ADIF                :
        case Mode_ADTS                :
                                        break;
        default                       : return true; //no file header test with other modes
    }

    //Tags
    if (!File__Tags_Helper::FileHeader_Begin())
        return false;

    //Testing
    if (Buffer_Offset+4>Buffer_Size)
        return false;
    if (CC4(Buffer+Buffer_Offset)==0x41444946) //"ADIF"
    {
        Mode=Mode_ADIF;
        File__Tags_Helper::Accept("ADIF");
        MustSynchronize=false;
    }

    return true;
}

//---------------------------------------------------------------------------
void File_Aac::FileHeader_Parse()
{
    switch (Mode)
    {
        case Mode_ADIF    : FileHeader_Parse_ADIF(); break;
        default           : ; //no file header test with other modes
    }
}

//---------------------------------------------------------------------------
void File_Aac::FileHeader_Parse_ADIF()
{
    adif_header();
    BS_Begin();
    raw_data_block();
    BS_End();

    FILLING_BEGIN();
        File__Tags_Helper::Finish();
    FILLING_END();
}

//***************************************************************************
// Buffer - Global
//***************************************************************************

//---------------------------------------------------------------------------
void File_Aac::Read_Buffer_Continue()
{
    if (Element_Size==0)
        return;

    switch(Mode)
    {
        case Mode_AudioSpecificConfig : Read_Buffer_Continue_AudioSpecificConfig(); break;
        case Mode_raw_data_block      : Read_Buffer_Continue_raw_data_block(); break;
        case Mode_ADIF                :
        case Mode_ADTS                : File__Tags_Helper::Read_Buffer_Continue(); break;
        default                       : ;
    }
}

//---------------------------------------------------------------------------
void File_Aac::Read_Buffer_Continue_AudioSpecificConfig()
{
    File__Analyze::Accept(); //We automaticly trust it

    BS_Begin();
    AudioSpecificConfig(0); //Up to the end of the block
    BS_End();

    Mode=Mode_raw_data_block; //Mode_AudioSpecificConfig only once
}

//---------------------------------------------------------------------------
void File_Aac::Read_Buffer_Continue_raw_data_block()
{
    if (Frame_Count>Frame_Count_Valid)
    {
        Skip_XX(Element_Size,                                   "Data");
        return; //Parsing completely only the 1st frame
    }

    BS_Begin();
    raw_data_block();
    BS_End();
    if (FrameIsAlwaysComplete && Element_Offset<Element_Size)
        Skip_XX(Element_Size-Element_Offset,                    "Unknown");

    FILLING_BEGIN();
        //Counting
        Frame_Count++;
        if (Frame_Count_NotParsedIncluded!=(int64u)-1)
            Frame_Count_NotParsedIncluded++;
        Element_Info1(Ztring::ToZtring(Frame_Count));

        //Filling
        if (!Status[IsAccepted])
            File__Analyze::Accept();
        if (Frame_Count>=Frame_Count_Valid)
        {
            //No more need data
            File__Analyze::Finish();
        }
    FILLING_END();
}

//***************************************************************************
// Buffer - Synchro
//***************************************************************************

//---------------------------------------------------------------------------
bool File_Aac::Synchronize()
{
    switch (Mode)
    {
        case Mode_Unknown     : if (Synchronize_LATM()) return true; Buffer_Offset=0; return Synchronize_ADTS();
        case Mode_ADTS        : return Synchronize_ADTS();
        case Mode_LATM        : return Synchronize_LATM();
        default               : return true; //No synchro
    }
}

//---------------------------------------------------------------------------
bool File_Aac::Synchronize_ADTS()
{
    //Tags
    bool Tag_Found;
    if (!File__Tags_Helper::Synchronize(Tag_Found))
        return false;
    if (Tag_Found)
        return true;

    //Synchronizing
    while (Buffer_Offset+6<=Buffer_Size)
    {
         while (Buffer_Offset+6<=Buffer_Size && (Buffer[Buffer_Offset  ]!=0xFF
                                              || (Buffer[Buffer_Offset+1]&0xF6)!=0xF0))
            Buffer_Offset++;

        if (Buffer_Offset+6<=Buffer_Size)//Testing if size is coherant
        {
            //Testing next start, to be sure
            int16u aac_frame_length=(CC3(Buffer+Buffer_Offset+3)>>5)&0x1FFF;
            if (IsSub && Buffer_Offset+aac_frame_length==Buffer_Size)
                break;
            if (File_Offset+Buffer_Offset+aac_frame_length!=File_Size-File_EndTagSize)
            {
                if (Buffer_Offset+aac_frame_length+2>Buffer_Size)
                    return false; //Need more data

                //Testing
                if (aac_frame_length<=7 || (CC2(Buffer+Buffer_Offset+aac_frame_length)&0xFFF6)!=0xFFF0)
                    Buffer_Offset++;
                else
                {
                    //Testing next start, to be sure
                    int16u aac_frame_length2=(CC3(Buffer+Buffer_Offset+aac_frame_length+3)>>5)&0x1FFF;
                    if (File_Offset+Buffer_Offset+aac_frame_length+aac_frame_length2!=File_Size-File_EndTagSize)
                    {
                        if (Buffer_Offset+aac_frame_length+aac_frame_length2+2>Buffer_Size)
                            return false; //Need more data

                        //Testing
                        if (aac_frame_length2<=7 || (CC2(Buffer+Buffer_Offset+aac_frame_length+aac_frame_length2)&0xFFF6)!=0xFFF0)
                            Buffer_Offset++;
                        else
                        {
                            //Testing next start, to be sure
                            int16u aac_frame_length3=(CC3(Buffer+Buffer_Offset+aac_frame_length+aac_frame_length2+3)>>5)&0x1FFF;
                            if (File_Offset+Buffer_Offset+aac_frame_length+aac_frame_length2+aac_frame_length3!=File_Size-File_EndTagSize)
                            {
                                if (Buffer_Offset+aac_frame_length+aac_frame_length2+aac_frame_length3+2>Buffer_Size)
                                    return false; //Need more data

                                //Testing
                                if (aac_frame_length3<=7 || (CC2(Buffer+Buffer_Offset+aac_frame_length+aac_frame_length2+aac_frame_length3)&0xFFF6)!=0xFFF0)
                                    Buffer_Offset++;
                                else
                                    break; //while()
                            }
                            else
                                break; //while()
                        }
                    }
                    else
                        break; //while()
                }
            }
            else
                break; //while()
        }
    }

    //Parsing last bytes if needed
    if (Buffer_Offset+6>Buffer_Size)
    {
        if (Buffer_Offset+5==Buffer_Size && (CC2(Buffer+Buffer_Offset)&0xFFF6)!=0xFFF0)
            Buffer_Offset++;
        if (Buffer_Offset+4==Buffer_Size && (CC2(Buffer+Buffer_Offset)&0xFFF6)!=0xFFF0)
            Buffer_Offset++;
        if (Buffer_Offset+3==Buffer_Size && (CC2(Buffer+Buffer_Offset)&0xFFF6)!=0xFFF0)
            Buffer_Offset++;
        if (Buffer_Offset+2==Buffer_Size && (CC2(Buffer+Buffer_Offset)&0xFFF6)!=0xFFF0)
            Buffer_Offset++;
        if (Buffer_Offset+1==Buffer_Size && CC1(Buffer+Buffer_Offset)!=0xFF)
            Buffer_Offset++;
        return false;
    }

    //Synched is OK
    Mode=Mode_ADTS;
    File__Tags_Helper::Accept("ADTS");
    return true;
}

//---------------------------------------------------------------------------
bool File_Aac::Synchronize_LATM()
{
    //Synchronizing
    while (Buffer_Offset+3<=Buffer_Size)
    {
         while (Buffer_Offset+3<=Buffer_Size && (Buffer[Buffer_Offset  ]!=0x56
                                              || (Buffer[Buffer_Offset+1]&0xE0)!=0xE0))
            Buffer_Offset++;

        if (Buffer_Offset+3<=Buffer_Size)//Testing if size is coherant
        {
            //Testing next start, to be sure
            int16u audioMuxLengthBytes=CC2(Buffer+Buffer_Offset+1)&0x1FFF;
            if (IsSub && Buffer_Offset+3+audioMuxLengthBytes==Buffer_Size)
                break;
            if (File_Offset+Buffer_Offset+3+audioMuxLengthBytes!=File_Size)
            {
                if (Buffer_Offset+3+audioMuxLengthBytes+3>Buffer_Size)
                    return false; //Need more data

                //Testing
                if ((CC2(Buffer+Buffer_Offset+3+audioMuxLengthBytes)&0xFFE0)!=0x56E0)
                    Buffer_Offset++;
                else
                {
                    //Testing next start, to be sure
                    int16u audioMuxLengthBytes2=CC2(Buffer+Buffer_Offset+3+audioMuxLengthBytes+1)&0x1FFF;
                    if (File_Offset+Buffer_Offset+3+audioMuxLengthBytes+3+audioMuxLengthBytes2!=File_Size)
                    {
                        if (Buffer_Offset+3+audioMuxLengthBytes+3+audioMuxLengthBytes2+3>Buffer_Size)
                            return false; //Need more data

                        //Testing
                        if ((CC2(Buffer+Buffer_Offset+3+audioMuxLengthBytes+3+audioMuxLengthBytes2)&0xFFE0)!=0x56E0)
                            Buffer_Offset++;
                        else
                            break; //while()
                    }
                    else
                        break; //while()
                }
            }
            else
                break; //while()
        }
    }


    //Synchronizing
    while (Buffer_Offset+2<=Buffer_Size && (Buffer[Buffer_Offset  ]!=0x56
                                         || (Buffer[Buffer_Offset+1]&0xE0)!=0xE0))
        Buffer_Offset++;
    if (Buffer_Offset+2>=Buffer_Size)
        return false;

    //Synched is OK
    Mode=Mode_LATM;
    File__Analyze::Accept("LATM");
    return true;
}

//---------------------------------------------------------------------------
bool File_Aac::Synched_Test()
{
    switch (Mode)
    {
        case Mode_ADTS        : return Synched_Test_ADTS();
        case Mode_LATM        : return Synched_Test_LATM();
        default               : return true; //No synchro
    }
}

//---------------------------------------------------------------------------
bool File_Aac::Synched_Test_ADTS()
{
    //Tags
    if (!File__Tags_Helper::Synched_Test())
        return false;

    //Must have enough buffer for having header
    if (Buffer_Offset+2>Buffer_Size)
        return false;

    //Quick test of synchro
    if ((CC2(Buffer+Buffer_Offset)&0xFFF6)!=0xFFF0)
        Synched=false;

    //We continue
    return true;
}

//---------------------------------------------------------------------------
bool File_Aac::Synched_Test_LATM()
{
    //Must have enough buffer for having header
    if (Buffer_Offset+2>Buffer_Size)
        return false;

    //Quick test of synchro
    if ((CC2(Buffer+Buffer_Offset)&0xFFE0)!=0x56E0)
        Synched=false;

    //We continue
    return true;
}

//***************************************************************************
// Buffer - Demux
//***************************************************************************

//---------------------------------------------------------------------------
#if MEDIAINFO_DEMUX
bool File_Aac::Demux_UnpacketizeContainer_Test()
{
    switch (Mode)
    {
        case Mode_ADTS        : return Demux_UnpacketizeContainer_Test_ADTS();
        case Mode_LATM        : return Demux_UnpacketizeContainer_Test_LATM();
        default               : return true; //No header
    }
}
bool File_Aac::Demux_UnpacketizeContainer_Test_ADTS()
{
    int16u aac_frame_length=(BigEndian2int24u(Buffer+Buffer_Offset+3)>>5)&0x1FFF; //13 bits
    Demux_Offset=Buffer_Offset+aac_frame_length;

    if (Demux_Offset>Buffer_Size && File_Offset+Buffer_Size!=File_Size)
        return false; //No complete frame

    Demux_UnpacketizeContainer_Demux();

    return true;
}
bool File_Aac::Demux_UnpacketizeContainer_Test_LATM()
{
    int16u audioMuxLengthBytes=BigEndian2int16u(Buffer+Buffer_Offset+1)&0x1FFF; //13 bits
    Demux_Offset=Buffer_Offset+3+audioMuxLengthBytes;

    if (Demux_Offset>Buffer_Size && File_Offset+Buffer_Size!=File_Size)
        return false; //No complete frame

    Demux_UnpacketizeContainer_Demux();

    return true;
}
#endif //MEDIAINFO_DEMUX

//***************************************************************************
// Buffer - Per element
//***************************************************************************

//---------------------------------------------------------------------------
bool File_Aac::Header_Begin()
{
    switch (Mode)
    {
        case Mode_ADTS        : return Header_Begin_ADTS();
        case Mode_LATM        : return Header_Begin_LATM();
        default               : return true; //No header
    }
}

//---------------------------------------------------------------------------
bool File_Aac::Header_Begin_ADTS()
{
    //There is no real header in ADTS, retrieving only the frame length
    if (Buffer_Offset+8>Buffer_Size) //size of adts_fixed_header + adts_variable_header
        return false;

    return true;
}

//---------------------------------------------------------------------------
bool File_Aac::Header_Begin_LATM()
{
    if (Buffer_Offset+3>Buffer_Size) //fixed 24-bit header
        return false;

    return true;
}

//---------------------------------------------------------------------------
void File_Aac::Header_Parse()
{
    switch (Mode)
    {
        case Mode_ADTS        : Header_Parse_ADTS(); break;
        case Mode_LATM        : Header_Parse_LATM(); break;
        default               : ; //No header
    }
}

//---------------------------------------------------------------------------
void File_Aac::Header_Parse_ADTS()
{
    //There is no "header" in ADTS, retrieving only the frame length
    int16u aac_frame_length=(BigEndian2int24u(Buffer+Buffer_Offset+3)>>5)&0x1FFF; //13 bits

    //Filling
    Header_Fill_Size(aac_frame_length);
    Header_Fill_Code(0, "adts_frame");
}

//---------------------------------------------------------------------------
void File_Aac::Header_Parse_LATM()
{
    int16u audioMuxLengthBytes;
    BS_Begin();
    Skip_S2(11,                                                 "syncword");
    Get_S2 (13, audioMuxLengthBytes,                            "audioMuxLengthBytes");
    BS_End();

    //Filling
    Header_Fill_Size(3+audioMuxLengthBytes);
    Header_Fill_Code(0, "LATM");
}

//---------------------------------------------------------------------------
void File_Aac::Data_Parse()
{
    if (FrameSize_Min>Header_Size+Element_Size)
        FrameSize_Min=Header_Size+Element_Size;
    if (FrameSize_Max<Header_Size+Element_Size)
        FrameSize_Max=Header_Size+Element_Size;

    if (Frame_Count>Frame_Count_Valid || CanFill)
    {
        Skip_XX(Element_Size,                                   "Data");
        FrameInfo.DTS+=float64_int64s(((float64)frame_length)*1000000000/sampling_frequency);
        FrameInfo.PTS=FrameInfo.DTS;
        return; //Parsing completely only the 1st frame
    }

    switch (Mode)
    {
        case Mode_ADTS        : Data_Parse_ADTS(); break;
        case Mode_LATM        : Data_Parse_LATM(); break;
        default               : ; //No header
    }

    FILLING_BEGIN();
        //Counting
        if (File_Offset+Buffer_Offset+Element_Size==File_Size)
            Frame_Count_Valid=Frame_Count; //Finish frames in case of there are less than Frame_Count_Valid frames
        Frame_Count++;
        if (Frame_Count_NotParsedIncluded!=(int64u)-1)
            Frame_Count_NotParsedIncluded++;
        Element_Info1(Ztring::ToZtring(Frame_Count));

        //Filling
        if ((Frame_Count>=Frame_Count_Valid || CanFill) && Config->ParseSpeed<1.0)
        {
            //No more need data
            switch (Mode)
            {
                case Mode_ADTS        :
                case Mode_LATM        :
                                        Fill();
                                        if (!IsSub)
                                            File__Tags_Helper::Finish(); break;
                default               : ; //No header
            }

        }
    FILLING_END();
}

//---------------------------------------------------------------------------
void File_Aac::Data_Parse_ADTS()
{
    //Parsing
    BS_Begin();
    adts_frame();
    BS_End();
}

//---------------------------------------------------------------------------
void File_Aac::Data_Parse_LATM()
{
    BS_Begin();
    AudioMuxElement();
    BS_End();
}

//***************************************************************************
// C++
//***************************************************************************

} //NameSpace

#endif //MEDIAINFO_AAC_YES
