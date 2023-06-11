#include <iostream>
#include "GUIInterface.h"
#include "Global.h"
#include "Interpreter.h"
#include "API.h"
#include "CatalogManager.h"
#include "RecordManager.h"
#include "IndexManager.h"
#include "BufferManager.h"

using namespace std;

//ʵ����
BufferManager* bm = NULL;
RecordManager* rm = NULL;
IndexManager* im = NULL;
CatalogManager* cm = NULL;
API* api = NULL;
Interpreter* it = NULL;

int Inited = 0;

int State = 0;
char Message[4096] = "";
int Time = 0;
int Counter = -1;
int QuitFlag = 0;

//������������ļ�, �ر�����MiniSQL
void Reset(void) {
	if (Inited) {
		Inited = 0;
		delete bm;
		delete rm;
		delete im;
		delete cm;
		delete api;
		delete it;
	}
	Remove(SchemasDirectory);
	Touch(SchemasDirectory);
	Remove(DataDirectory);
	Touch(DataDirectory);
	Remove(IndexDirectory);
	Touch(IndexDirectory);
	Remove(ResultDirectory);
	Touch(ResultDirectory);
}

//��ʼ��
void Init(void) {
	if (Inited)
		return;
	Inited = 1;
	bm = new BufferManager;
	rm = new RecordManager(*bm);
	im = new IndexManager(*bm);
	cm = new CatalogManager();
	api = new API(*cm, *rm, *im);
	it = new Interpreter(*api, *cm);
}

//ִ�е���ָ��
void Exec(char* str) {
	if (!Inited)
		return;
	const ReturnInfo& ReturnValue = it->ExecCommand(str);
	State = ReturnValue.State;
	memcpy(Message, ReturnValue.Message.c_str(), ReturnValue.Message.length()); Message[ReturnValue.Message.length()] = '\0';
	Time = ReturnValue.Time;
	Counter = ReturnValue.Counter;
	QuitFlag = ReturnValue.Quit;
	if (QuitFlag) {
		Inited = 0;
		delete bm;
		delete rm;
		delete im;
		delete cm;
		delete api;
		delete it;
	}
}

//��ȡ��һ��ָ��ִ�н��, 0: ����, 1: �ɹ�
int GetState(void) {
	return State;
}

//��ȡ��һ��ָ�����Ϣ
char* GetMessage(void) {
	return Message;
}

//��ȡ��һ��ָ������ʱ��, ��CLOCKΪ��λ
int GetTime(void) {
	return Time;
}

//��ȡInterpreter������
int GetCounter(void) {
	return Counter;
}

//��ȡ��������״̬, ��ִ�й�quitָ��, Ӧ����0
int Quit(void) {
	return QuitFlag;
}
