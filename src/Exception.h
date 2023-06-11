#pragma once
#include <exception>

using namespace std;

//***********************************************************全局异常定义***********************************************************

//语法错误, 由Interpreter抛出
class SyntaxError : public exception {
public:
	SyntaxError(void) {}
	~SyntaxError(void) {}
	virtual const char* what() const throw() {
		return "Syntax Error: You have an error in your SQL syntax.";
	}
};

//表名重复，由Interpreter抛出
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

//表不存在，由Interpreter抛出
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

//索引重复，由Interpreter抛出
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

//索引不存在，由Interpreter抛出
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

//属性名重复，由Interpreter抛出
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

//属性名不存在，由Interpreter抛出
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

//值不匹配，由Interpreter抛出
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

//索引定义在非Unique属性上，由Interpreter抛出
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

//遇到重复属性而插入失败，由Index Manager或Record Manager检查, 并由API抛出
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

//文件打开失败
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