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

//实例化
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

//清空所有数据文件, 关闭整个MiniSQL
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

//初始化
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

//执行单条指令
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

//获取上一条指令执行结果, 0: 报错, 1: 成功
int GetState(void) {
	return State;
}

//获取上一条指令返回信息
char* GetMessage(void) {
	return Message;
}

//获取上一条指令运行时间, 以CLOCK为单位
int GetTime(void) {
	return Time;
}

//获取Interpreter计数器
int GetCounter(void) {
	return Counter;
}

//获取程序运行状态, 如执行过quit指令, 应返回0
int Quit(void) {
	return QuitFlag;
}
