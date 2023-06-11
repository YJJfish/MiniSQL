#pragma once
#include <exception>

using namespace std;

//***********************************************************ȫ���쳣����***********************************************************

//�﷨����, ��Interpreter�׳�
class SyntaxError : public exception {
public:
	SyntaxError(void) {}
	~SyntaxError(void) {}
	virtual const char* what() const throw() {
		return "Syntax Error: You have an error in your SQL syntax.";
	}
};

//�����ظ�����Interpreter�׳�
class DuplicatedTable : public exception {
public:
	DuplicatedTable(string TableName) :TableName(TableName), 
		Message("Invalid Identifier: Table " + TableName + " already exists.") {}
	~DuplicatedTable(void) {}
	virtual const char* what() const throw() {
		return Message.c_str();
	}
private:
	string TableName;
	string Message;
};

//�����ڣ���Interpreter�׳�
class NonexistentTable : public exception {
public:
	NonexistentTable(string TableName) :TableName(TableName), 
		Message("Invalid Identifier: Table " + TableName + " doesn't exist.") {}
	~NonexistentTable(void) {}
	virtual const char* what() const throw() {
		return Message.c_str();
	}
private:
	string TableName;
	string Message;
};

//�����ظ�����Interpreter�׳�
class DuplicatedIndex : public exception {
public:
	DuplicatedIndex(string IndexName) :IndexName(IndexName), 
		Message(("Invalid Identifier: Index " + IndexName + " already exists.")) {}
	~DuplicatedIndex(void) {}
	virtual const char* what() const throw() {
		return Message.c_str();
	}
private:
	string IndexName;
	string Message;
};

//���������ڣ���Interpreter�׳�
class NonexistentIndex : public exception {
public:
	NonexistentIndex(string IndexName) :IndexName(IndexName), 
		Message("Invalid Identifier: Index " + IndexName + " doesn't exist.") {}
	~NonexistentIndex(void) {}
	virtual const char* what() const throw() {
		return Message.c_str();
	}
private:
	string IndexName;
	string Message;
};

//�������ظ�����Interpreter�׳�
class DuplicatedAttr : public exception {
public:
	DuplicatedAttr(string TableName, string AttrName) :TableName(TableName), AttrName(AttrName),
		Message("Invalid Identifier: Duplicated attribute " + AttrName + " for table " + TableName + ".") {}
	~DuplicatedAttr(void) {}
	virtual const char* what() const throw() {
		return Message.c_str();
	}
private:
	string TableName;
	string AttrName;
	string Message;
};

//�����������ڣ���Interpreter�׳�
class NonexistentAttr : public exception {
public:
	NonexistentAttr(string TableName, string AttrName) :TableName(TableName), AttrName(AttrName), 
		Message("Invalid Identifier: Table " + TableName + " has no attribute named " + AttrName + ".") {}
	~NonexistentAttr(void) {}
	virtual const char* what() const throw() {
		return Message.c_str();
	}
private:
	string TableName;
	string AttrName;
	string Message;
};

//ֵ��ƥ�䣬��Interpreter�׳�
class InvalidValue: public exception {
public:
	InvalidValue(string AttrName, string Value) :AttrName(AttrName), Value(Value), 
		Message("Invalid Value: Value " + Value + " doesn't match the attrbute " + AttrName + ".") {}
	~InvalidValue(void) {}
	virtual const char* what() const throw() {
		return Message.c_str();
	}
private:
	string AttrName;
	string Value;
	string Message;
};

//���������ڷ�Unique�����ϣ���Interpreter�׳�
class InvalidAttrForIndex: public exception {
public:
	InvalidAttrForIndex(string TableName, string AttrName) :TableName(TableName), AttrName(AttrName),
		Message("Invalid Attribute For Index: Attribute " + AttrName + " in table " + TableName + " has no UNIQUE constraint.") {}
	~InvalidAttrForIndex(void) {}
	virtual const char* what() const throw() {
		return Message.c_str();
	}
private:
	string TableName;
	string AttrName;
	string Message;
};

//�����ظ����Զ�����ʧ�ܣ���Index Manager��Record Manager���, ����API�׳�
class InsertionError: public exception {
public:
	InsertionError(string AttrName, string Value) :AttrName(AttrName), Value(Value),
		Message("Insertion Error: Value " + Value + " is duplicated for UNIQUE attribute " + AttrName + ".") {}
	~InsertionError(void) {}
	virtual const char* what() const throw() {
		return Message.c_str();
	}
private:
	string AttrName;
	string Value;
	string Message;
};

//�ļ���ʧ��
class InvalidFile : public exception {
public:
	InvalidFile(string Filename) :Filename(Filename),
		Message("File " + Filename + " doesn't exist.") {};
	~InvalidFile(void) {}
	virtual const char* what() const throw() {
		return Message.c_str();
	}
private:
	string Filename;
	string Message;
};