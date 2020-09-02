#pragma once
#include "MediaWriter.h"
class CMediaWriterTs :
	public CMediaWriter
{
public:
	CMediaWriterTs(CMediaStream& stream);
	~CMediaWriterTs();

	virtual bool onWriteHeader();
	virtual bool onWriteVideo(const CMediaStream::H264Frame * frame);
	virtual bool onWriteAudio(const CMediaStream::AUDFrame * frame);

#pragma pack(push,1)
	//http://www.cnblogs.com/tocy/p/media_container_6-mpegts.html
	struct	TS_Packet_Header
	{
		uint32_t	sync_byte : 8;	//��ͬ���ֽڣ����̶�Ϊ0x47;���ֽ��ɽ�����ʶ��ʹ��ͷ����Ч���ؿ��໥���롣
		uint32_t	transport_error_indicator : 1;	//����������־������1����ʾ����صĴ������������һ�����ɾ����Ĵ���λ��������1���ڴ��󱻾���֮ǰ��������Ϊ0��
		uint32_t	payload_unit_start_indicator : 1;	//��������ʼ��־����Ϊ1ʱ����ʾ��ǰTS������Ч�غ��а���PES����PSI����ʼλ�ã���ǰ4���ֽ�֮�����һ�������ֽڣ������ֵΪ��������ֶεĳ���length�������Ч�غɿ�ʼ��λ��Ӧ��ƫ��1+[length]���ֽڡ�
		uint32_t	transport_priority : 1;	//���������ȼ���־������1��������ǰTS�������ȼ�������������ͬPID�� ����λû�б��á�1����TS���ߡ�
		uint32_t	PID : 13;	// ָʾ�洢�������Ч���������ݵ����͡�PIDֵ0x0000��0x000F����������0x0000ΪPAT������0x0001ΪCAT������0x1fffΪ���鱣�������հ���
		uint32_t	transport_scrambling_control : 2;	//�����ſ��Ʊ�־������ʾTS��������Ч���صļ���ģʽ���հ�Ϊ��00��������������ͷ�а��������ֶΣ���Ӧ�����ܡ�����ȡֵ�������û��Զ���ġ�
		uint32_t	adaptation_field_control : 2;	//����������Ʊ�־������ʾ��ͷ�Ƿ��е����ֶλ���Ч���ء���00��ΪISO/IECδ��ʹ�ñ�������01��������Ч�غɣ��޵����ֶΣ���10�� ����Ч�غɣ����������ֶΣ���11�� �����ֶκ�Ϊ��Ч�غɣ������ֶ��е�ǰһ���ֽڱ�ʾ�����ֶεĳ���length����Ч�غɿ�ʼ��λ��Ӧ��ƫ��[length]���ֽڡ��հ�ӦΪ��10����
		uint32_t	continuity_counter : 4;	//�������Լ�������������ÿһ��������ͬPID��TS����������ӣ������ﵽ���ֵ���ֻظ���0����ΧΪ0~15��
	};
	//��Ŀ������Program Association Table (PAT) 0x0000	TS���а���һ�����߶��PAT��PAT����PIDΪ0x0000��TS�����ͣ���������Ϊ���õ�ÿһ·�������ṩ���������Ľ�Ŀ�ͽ�Ŀ��ţ��Լ���Ӧ��Ŀ��PMT��λ�ü�PMT��TS����PIDֵ��ͬʱ���ṩNIT��λ�ã���NIT��TS����PID��ֵ��
	//��Ŀӳ���Program Map Tables (PMT)	PMT�ڴ�����������ָʾ���ĳһ�׽�Ŀ����Ƶ����Ƶ�������ڴ������е�λ�ã�����Ӧ��TS����PIDֵ���Լ�ÿ·��Ŀ�Ľ�Ŀʱ�Ӳο���PCR���ֶε�λ�á� 
	//�������ձ�Conditional Access Table (CAT) 0x0001
	//������Ϣ��Network Information Table(NIT) 0x0010
	//������������Transport Stream Description Table(TSDT) 0x02

	struct	TS_PAT
	{
		uint64_t	table_id : 8;	//�̶�Ϊ0x00����־�Ǹñ���PAT
		uint64_t	section_syntax_indicator : 1;	//���﷨��־λ���̶�Ϊ1
		uint64_t	zero : 1;	//0
		uint64_t	reserved_1 : 2;	//����λ
		uint64_t	section_length : 12;	//��ʾ��������õ��ֽ���������CRC32
		uint64_t	transport_stream_id : 16;	//�ô�������ID��������һ��������������·���õ���
		uint64_t	reserved_2 : 2;	//����λ
		uint64_t	version_number : 5;	//��Χ0-31����ʾPAT�İ汾��
		uint64_t	current_next_indicator : 1;	//���͵�PAT�ǵ�ǰ��Ч������һ��PAT��Ч
		uint64_t	section_number : 8;	//�ֶεĺ��롣PAT���ܷ�Ϊ��δ��䣬��һ��Ϊ00���Ժ�ÿ���ֶμ�1����������256���ֶ�
		uint64_t	last_section_number : 8;	//��ʾPAT���һ���ֶεĺ��롣
		uint32_t	CRC_32 : 32;
	};
	struct	TS_PAT_PROGRAM
	{
		uint32_t	program_number : 16;	//��Ŀ��
		uint32_t	reserved_3 : 3;
		union {
			uint32_t	program_map_PID : 13;	//��Ŀӳ���PMT����PID�ţ���Ŀ��Ϊ���ڵ���1ʱ����Ӧ��IDΪprogram_map_PID��һ��PAT�п����ж��program_map_PID��
			uint32_t	network_PID : 13;	//������Ϣ��NIT����PID,��Ŀ��Ϊ0ʱ��ӦIDΪnetwork_PID��
		};
	};

	struct TS_PMT
	{
		uint64_t	table_id : 8;	//�̶�Ϊ0x02����־�ñ���PMT ��
		uint64_t	section_syntax_indicator : 1;	//����PMT������Ϊ1 ��
		uint64_t	zero : 1;	//0
		uint64_t	reserved_1 : 2;	//
		uint64_t	section_length : 12;	//��ʾ����ֽں������õ��ֽ���������CRC32 ��
		uint64_t	program_number : 16;	//��ָ���ý�Ŀ��Ӧ�ڿ�Ӧ�õ�Program map PID ��
		uint64_t	reserved_2 : 2;
		uint64_t	version_number : 5;	//ָ��PMT �İ汾�š�
		uint64_t	current_next_indicator : 1;	//����λ�á�1��ʱ����ǰ���͵�Program map section���ã�����λ�á�0��ʱ��ָʾ��ǰ���͵�Program map section�����ã���һ��TS����Programmap section ��Ч��
		uint64_t	section_number : 8;	//������Ϊ0x00����ΪPMT�����ʾһ��service����Ϣ��һ��section �ĳ����㹻����
		uint64_t	last_section_number : 8;	//�����ֵ����0x00 ��

		uint32_t	reserved_3 : 3;
		uint32_t	PCR_PID : 13;	//��Ŀʱ�Ӳο�����TS�����PID����Ŀ�а�����ЧPCR�ֶεĴ�������PID ��
		uint32_t	reserved_4 : 4;
		uint32_t	program_info_length : 12;	//12bit��ǰ��λΪ00������ָ���������Խ�Ŀ��Ϣ��������byte ����

		uint32_t	CRC_32 : 32;
	};
	struct	TS_PMT_STREAM
	{
		uint64_t	stream_type : 8;	//ָʾ�ض�PID�Ľ�ĿԪ�ذ������͡��ô�PID��elementary PID ָ����
		uint64_t	reserved_5 : 3;
		uint64_t	elementary_PID : 13;
		uint64_t	reserved_6 : 4;
		uint64_t	ES_info_length : 12;
	};
	//PES�������
	//http://blog.csdn.net/cabbage2008/article/details/49848937
	//http://blog.csdn.net/u013354805/article/details/51591229
	struct TS_PES
	{
		uint32_t	packet_start_code_prefix : 24;
		uint32_t	stream_id : 8;

		uint16_t	PES_packet_length;

		uint8_t		fix_bit : 2;		//��־λ���̶�Ϊ 10
		uint8_t		PES_scrambling_control : 2;		//PES ���ſ���
		uint8_t		PES_priority : 1;		//PES ���ȼ�
		uint8_t		data_alignment_indicator : 1;		//���ݶ�λָʾ��
		uint8_t		copyright : 1;		//��Ȩ
		uint8_t		original_or_copy : 1;		//ԭʼ�Ļ��Ƶ�

		uint8_t		PTS_DTS_flags : 2;		//ʱ������
		uint8_t		ESCR_flag : 1;
		uint8_t		ES_rate_flag : 1;
		uint8_t		DSM_trick_mode_flag : 1;
		uint8_t		additional_copy_info_flag : 1;
		uint8_t		PES_CRC_flag : 1;
		uint8_t		PES_extension_flag : 1;

		uint8_t		PES_header_data_length;
	};

#pragma pack(pop)
	struct TS_Stream
	{
		uint16_t	programNumber;
		uint16_t	stream_type;	//ָʾ�ض�PID�Ľ�ĿԪ�ذ������͡��ô�PID��elementary PID ָ����
		uint16_t	elementary_PID;
		vector<uint8_t>	ES_info;
	};
	map<uint16_t, uint8_t> m_pidCounter;
	map<uint16_t, uint16_t> m_patProgs;
	vector<TS_Stream> m_pmtStreams;
	string		m_videoCache;
	string		m_audioCache;
	int64_t	m_audioTime;

	uint8_t m_pack[188];
	int32_t m_startBit;
	uint32_t crc32(const uint8_t* data, uint32_t size);
	void setTsPacketHeader(uint16_t pid, bool isStart = true, uint8_t startBytes = 0, int64_t pcr = -1);
	void makeTsPat();
	void makeTsPmt(uint16_t programNumber);
	void writeBits(uint8_t bits, uint32_t data);
};

