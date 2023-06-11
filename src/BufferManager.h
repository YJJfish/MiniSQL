#pragma once
#include "Global.h"
#include <iostream>
using namespace std;

//��
class Block {
public:
	char Content[BlockSize + 1];//�������, �̶�ΪBlockSize���ֽڣ����е�BlockSize+1���ֽڼ�Content[BlockSize]����'\0'
	int GetOffset(void);        //��ѯ��Block���ļ��ڵ�ƫ�Ƶ�ַ
	string GetFilename(void);   //��ѯ��Block��Ӧ���ļ���
	
private:
	bool Occupied;           //��ǰ�Ƿ�ռ��
	bool Hot;                //�Ƿ�λ��������������
	int BirthTime;           //�����ڴ��ʱ��
	int VisitTime;           //�������ʱ��
	string Filename;         //�ļ���
	int Offset;              //�����ļ��ڵ�ƫ����, ��0��ʼ����. �������0ʱ��ʾ���ļ���ͷ����. ����<=���ļ��ܿ���-1.
	bool Changed;            //�Ƿ��޸ġ�δ���޸Ĺ��Ŀ鱻����ʱ��������д�ش���
	Block* Last, * Next;     //����
	Block(int Hot);          //���캯��
	~Block(void);            //��������
	bool Write(void);                         //д��Ӳ��, ����ֵ��ʾ�ɹ����
	bool Read(string Filename, int Offset);   //��Ӳ�̶�ȡ, ����ֵ��ʾ�ɹ����

	friend class BufferManager;
};



//������
class BufferManager {
public:
	BufferManager(void);     //���캯��
	~BufferManager(void);    //��������

	/*
	 * ȡ��ĳһ������ڴ�. 
	 * ������ڻ��������Ѵ��ڣ���ֱ�ӷ���ָ�룻�������ڣ����ȡ���ٷ���ָ��.
	 * ��������������������滻��������������.
	 * ���ļ�������, ���ļ���С���ǿ��������, ��ƫ��������ָ���ļ��Ĵ�С, ����NULL
	 */
	Block* GetBlock(string Filename, int Offset);

	/*
	 * �ڸ��ļ�ĩβ׷��һ���飬����ȡ������أ����ؿ��ָ��
	 * �����ļ��Ĵ�С���ǿ��������, ����NULL
	 * ���Filename��λ�������ڵ��ļ�, ����ڴ������½����ļ�, ��׷�ӿ�
	 */
	Block* AppendBlock(string Filename);

	/*
	 * ���������ڵ�ָ����ǿ������������
	 * ������ĩβ�Ŀ������������, ��Ϊ������������ͷ�ڵ�
	 * �������������ڻ�û������������������False. ���򷵻�True
	 */
	bool HotBlock(Block* Obj);

	/*
	 * ÿ�η��ʸ�Blockʱ����øú���, �������»��������Ϳ����Ϣ
	 * RW=false��ʾ��������VisitTime; RW=true��ʾд������VisitTime��Changed
	 */
	void VisitBlock(Block* Obj, bool RW);

	/*
	 * Debug�ã���������������Ϣ
	 */
	void Debug(void);

private:
	Block* NewBlock; //����������ͷָ��
	Block* OldBlock; //����������ͷָ��
};
