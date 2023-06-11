#pragma once
#include <iostream>
#include "Global.h"
#include "CatalogManager.h"
#include "API.h"
using namespace std;

//Interpreter������Ϣ
class ReturnInfo {
public:
    int Quit;       //�Ƿ�����quit
    int State;      //��һ��ָ��ִ�����, 0: Failure; 1: Success
    string Message; //��һ��ָ�����Ϣ
    int Time;       //��һ��ָ��ִ����ʱ, ��λΪCLOCK
    int Counter;	//Interpreter�ڲ�ά���ļ�ʱ��. ִ�к�Counter - ִ��ǰCounter = ����ָ������Ĳ�ѯ�������
    ReturnInfo(void) :Quit(0), State(0), Message(""), Time(0), Counter(-1) {}
    ~ReturnInfo(void) {}
};

class Interpreter {
public:
    Interpreter(API& api, CatalogManager& cm) : api(api), cm(cm), ReturnValue() {}
    ~Interpreter() {}

    /*
     * ������ExecCommand
     * ����������ָ��s
     * ����ֵ��ָ��ִ�к�����з�����Ϣ
     * ����ָ�����apiģ��Ľӿ�ִ��ָ��
     */
    const ReturnInfo& ExecCommand(string s);

private:
    CatalogManager& cm;
    API& api;
    ReturnInfo ReturnValue;

    /*
     * ������ExecFile
     * �����ⲿ�ļ�����ָ�����ExecCommand
     * ����ֵ��bool, �Ƿ�ִ�е��ⲿ�ļ���Quit
     */
    bool ExecFile(string FileName);
};
