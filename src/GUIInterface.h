/*
 * ���ļ�����C++����ָ̨���������PythonGUI�Ľӿں���
 * ͨ����̬���ӿ�, ��Python�ܹ�����C++������
 * ����Python��֧��C++��string�࣬������Ҫ���ַ�������ʵ�֡�
 */

#pragma once
#include <iostream>

using namespace std;

#define IMPORT_DLL extern "C" _declspec(dllimport) //�����ⲿ����

//������������ļ�, ��������MiniSQL
IMPORT_DLL void Reset(void);

//��ʼ��
IMPORT_DLL void Init(void);

//ִ�е���ָ��
IMPORT_DLL void Exec(char* str);

//��ȡ��һ��ָ��ִ�н��, 0: ����, 1: �ɹ�
IMPORT_DLL int GetState(void);

//��ȡ��һ��ָ�����Ϣ
IMPORT_DLL char* GetMessage(void);

//��ȡ��һ��ָ������ʱ��, ��CLOCKΪ��λ
IMPORT_DLL int GetTime(void);

//��ȡInterpreter������
IMPORT_DLL int GetCounter(void);

//��ȡ��������״̬, ��ִ�й�quitָ��, Ӧ����0
IMPORT_DLL int Quit(void);

