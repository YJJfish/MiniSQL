#pragma once
#include <iostream>
#include "Global.h"
#include "CatalogManager.h"
#include "API.h"
using namespace std;

//Interpreter返回信息
class ReturnInfo {
public:
    int Quit;       //是否遇到quit
    int State;      //上一次指令执行情况, 0: Failure; 1: Success
    string Message; //上一次指令返回信息
    int Time;       //上一次指令执行用时, 单位为CLOCK
    int Counter;	//Interpreter内部维护的计时器. 执行后Counter - 执行前Counter = 这条指令产生的查询结果个数
    ReturnInfo(void) :Quit(0), State(0), Message(""), Time(0), Counter(-1) {}
    ~ReturnInfo(void) {}
};

class Interpreter {
public:
    Interpreter(API& api, CatalogManager& cm) : api(api), cm(cm), ReturnValue() {}
    ~Interpreter() {}

    /*
     * 函数：ExecCommand
     * 参数：单条指令s
     * 返回值：指令执行后的所有返回信息
     * 解析指令并调用api模块的接口执行指令
     */
    const ReturnInfo& ExecCommand(string s);

private:
    CatalogManager& cm;
    API& api;
    ReturnInfo ReturnValue;

    /*
     * 函数：ExecFile
     * 解析外部文件，按指令调用ExecCommand
     * 返回值：bool, 是否执行到外部文件的Quit
     */
    bool ExecFile(string FileName);
};
