/*
 * 本文件定义C++控制台指令解析器和PythonGUI的接口函数
 * 通过动态链接库, 让Python能够调用C++函数。
 * 由于Python不支持C++的string类，所以需要用字符数组来实现。
 */

#pragma once
#include <iostream>

using namespace std;

#define IMPORT_DLL extern "C" _declspec(dllimport) //允许被外部调用

//清空所有数据文件, 重启整个MiniSQL
IMPORT_DLL void Reset(void);

//初始化
IMPORT_DLL void Init(void);

//执行单条指令
IMPORT_DLL void Exec(char* str);

//获取上一条指令执行结果, 0: 报错, 1: 成功
IMPORT_DLL int GetState(void);

//获取上一条指令返回信息
IMPORT_DLL char* GetMessage(void);

//获取上一条指令运行时间, 以CLOCK为单位
IMPORT_DLL int GetTime(void);

//获取Interpreter计数器
IMPORT_DLL int GetCounter(void);

//获取程序运行状态, 如执行过quit指令, 应返回0
IMPORT_DLL int Quit(void);

