#pragma once
#include <iostream>
#include <fstream>
#include <sstream>
#include <io.h>

using namespace std;

//***********************************************************全局常数定义***********************************************************

//缓冲区一个块的大小, 单位为Byte
#define BlockSize 4096

//缓冲区块的个数
#define BlockNum 100

//老生代块的个数。当OldBlockNum等于BlockNum时，算法退化成普通的LRU
#define OldBlockNum 37

//一个块从老生代缓冲池进入新生代缓冲池至少停留的时间, 单位为毫秒
#define OldBlockTime 1000

//空闲字节占位符
#define Placeholder '\0'

//元组数据文件目录
#define DataDirectory "./Table/"

//模式数据文件目录
#define SchemasDirectory "./Catalog/"

//索引数据文件目录
#define IndexDirectory "./Index/"

//查询结果文件目录
#define ResultDirectory "./Result/"

//查询条件op的编码
#define eq 0
#define neq 1
#define les 2
#define greater 3
#define leq 4
#define geq 5

//***********************************************************全局类型定义***********************************************************

#define SQL_INT 0
#define SQL_FLOAT 1
#define SQL_CHAR 2

//表的模式信息，由Catalog Manager、API、Record Manager、Index Manager使用
//文件的读取和写入由Catalog Manager负责，并由API接收、传给Record Manager、Index Manager
//假设每个表必有一个单属性主键，Unique约束也只能建立在单属性上
class TableSchema {
public:
    string TableName;				//表名。长度限制为256字节(不含末尾'\0')
    unsigned char NumAttributes;	//属性个数, 占1字节. 1<=属性个数<=255 (实验指导书要求32个)
    string* AttributeName;			//表的各个属性名。使用动态内存分配数组空间。每个string长度限制为256字节(不含末尾'\0')
    bool* Unique;					//表的属性是否Unique，占NumAttributes个字节
    unsigned char* Type;			//每种属性的类别, 支持SQL_INT、SQL_FLOAT、SQL_CHAR
    unsigned char* Length;			//如果是char类别, 还需要记录其长度；其他类型，赋0即可。这种设计可用于日后扩展, 例如Type=SQL_INT时，Length决定整数的范围(short int, long int等)
    unsigned char Primary;			//标识第几个属性是主键

    TableSchema(void) : TableName(""), NumAttributes(0), AttributeName(NULL), Unique(NULL), Type(NULL), Length(NULL), Primary(0) {}
    ~TableSchema(void) {
        if (AttributeName)
            delete[] AttributeName;
        if (Unique)
            delete[] Unique;
        if (Type)
            delete[] Type;
        if (Length)
            delete[] Length;
    }
    //清除内存
    void Free(void) {
        if (AttributeName)
            delete[] AttributeName;
        if (Unique)
            delete[] Unique;
        if (Type)
            delete[] Type;
        if (Length)
            delete[] Length;
        TableName = "";
        NumAttributes = 0;
        Primary = 0;
        AttributeName = NULL;
        Unique = NULL;
        Type = NULL;
        Length = NULL;
    }
    //从给定的文件流中读取. fin必须用fin.open(<文件名>, ios::in | ios::binary)的方式打开
    void Read(fstream& fin) {
        Free();
        unsigned char len = 0;
        char buffer[257] = { 0 };
        //读TableName
        fin.read((char*)&len, 1);
        fin.read(buffer, len); buffer[len] = 0;
        this->TableName = string(buffer);
        //读NumAttributes
        fin.read((char*)&(this->NumAttributes), 1);
        //读AttributeName
        this->AttributeName = new string[this->NumAttributes];
        for (int i = 0; i < this->NumAttributes; i++) {
            fin.read((char*)&len, 1);
            fin.read(buffer, len); buffer[len] = 0;
            this->AttributeName[i] = string(buffer);
        }
        //读Unique
        this->Unique = new bool[this->NumAttributes];
        fin.read((char*)this->Unique, this->NumAttributes);
        //读Type
        this->Type = new unsigned char[this->NumAttributes];
        fin.read((char*)this->Type, this->NumAttributes);
        //读Length
        this->Length = new unsigned char[this->NumAttributes];
        fin.read((char*)this->Length, this->NumAttributes);
        //读Primary
        fin.read((char*)&(this->Primary), 1);
    }
    //写入指定的文件流. fout必须用fin.open(<文件名>, ios::out | ios::in | ios::binary)的方式打开
    void Write(fstream& fout) const{
        unsigned char len = 0;
        //写TableName
        len = this->TableName.length();
        fout.write((char*)&len, 1);
        fout.write(this->TableName.c_str(), len);
        //写NumAttributes
        fout.write((char*)&(this->NumAttributes), 1);
        //写AttributeName
        for (int i = 0; i < this->NumAttributes; i++) {
            len = this->AttributeName[i].length();
            fout.write((char*)&len, 1);
            fout.write(this->AttributeName[i].c_str(), len);
        }
        //写Unique
        fout.write((char*)this->Unique, this->NumAttributes);
        //写Type
        fout.write((char*)this->Type, this->NumAttributes);
        //写Length
        fout.write((char*)this->Length, this->NumAttributes);
        //写Primary
        fout.write((char*)&(this->Primary), 1);
    }

    //返回table表第AttrNum个属性所占用的字节大小
    int AttrSize(int AttrNum) const {
        return (Type[AttrNum] == SQL_INT || Type[AttrNum] == SQL_FLOAT) ? 4 : Length[AttrNum];
    }

    //返回table表单个元组所占用的字节大小
    int RecordSize(void) const {
        int sum = 0;
        for (int i = 0; i < NumAttributes; i++) {
            sum += AttrSize(i);
        }
        return (sum < 4) ? 5 : sum + 1;
    }

    //返回table表第AttrNum个属性在整个元组中的字节偏移
    int AttrOffset(int AttrNum) const {
        int sum = 0;
        for (int i = 0; i < AttrNum; i++) {
            sum += AttrSize(i);
        }
        return sum + 1;
    }

    friend class API;
    friend class Interpreter;
    friend class CatelogManager;
    friend class RecordManager;
    friend class IndexManager;
};




//索引的模式信息，由Catalog Manager、API、Index Manager使用
//文件的读取和写入由Catalog Manager负责，并由API接收、传给Index Manager
//假设索引只能建立在具有Unique约束的单属性上
class IndexSchema {
public:
    string IndexName;				//索引名。长度限制为256字节(不含末尾'\0')
    string TableName;				//表名。长度限制为256字节(不含末尾'\0')
    string AttributeName;			//属性名。长度限制为256字节(不含末尾'\0')
    unsigned char AttributeNumber;	//属性编号, 表示这个属性是原表中的第几个属性. 从0记起. 属于冗余数据, 但是可以加快相关流程
    unsigned char Type;				//属性的类别. 属于冗余数据, 但是可以加快相关流程
    unsigned char Length;			//属性的长度(仅对char而言). 属于冗余数据, 但是可以加快相关流程


    IndexSchema(void) :IndexName(""), TableName(""), AttributeName(""), AttributeNumber(0), Type(-1), Length(0) {}
    ~IndexSchema(void) {}
    //从给定的文件流中读取. fin必须用fin.open(<文件名>, ios::in | ios::binary)的方式打开
    void Read(fstream& fin) {
        unsigned char len = 0;
        char buffer[257] = { 0 };
        //读IndexName
        fin.read((char*)&len, 1);
        fin.read(buffer, len); buffer[len] = 0;
        this->IndexName = string(buffer);
        //读TableName
        fin.read((char*)&len, 1);
        fin.read(buffer, len); buffer[len] = 0;
        this->TableName = string(buffer);
        //读AttributeName
        fin.read((char*)&len, 1);
        fin.read(buffer, len); buffer[len] = 0;
        this->AttributeName = string(buffer);
        //读AttributeNumber
        fin.read((char*)&(this->AttributeNumber), 1);
        //读Type
        fin.read((char*)&(this->Type), 1);
        //读Length
        fin.read((char*)&(this->Length), 1);
    }
    //写入指定的文件流. fout必须用fin.open(<文件名>, ios::out | ios::in | ios::binary)的方式打开
    void Write(fstream& fout) const{
        unsigned char len = 0;
        //写IndexName
        len = this->IndexName.length();
        fout.write((char*)&len, 1);
        fout.write(this->IndexName.c_str(), len);
        //写TableName
        len = this->TableName.length();
        fout.write((char*)&len, 1);
        fout.write(this->TableName.c_str(), len);
        //写AttributeName
        len = this->AttributeName.length();
        fout.write((char*)&len, 1);
        fout.write(this->AttributeName.c_str(), len);
        //写AttributeNumber
        fout.write((char*)&(this->AttributeNumber), 1);
        //写Type
        fout.write((char*)&(this->Type), 1);
        //写Length
        fout.write((char*)&(this->Length), 1);
    }

    //返回该索引的属性所占用的字节大小
    int AttrSize(void) const {
        return (Type == SQL_INT || Type == SQL_FLOAT) ? 4 : Length;
    }

    friend class API;
    friend class Interpreter;
    friend class CatelogManager;
    friend class RecordManager;
    friend class IndexManager;
};

//select语句临时表类型
//节省内存, 减少不必要的磁盘IO, 所以只记录块偏移量/字节偏移量
//使用链表存储, 因为查询前并不知晓查询结果包含几个元组

class Pos {
public:
    unsigned short BlockOffset;
    unsigned short ByteOffset;
    Pos* Next;
    Pos(void) :BlockOffset(-1), ByteOffset(-1), Next(0) {}
    ~Pos(void) {}
};

//delete语句临时表类型
//和class Pos不一样，这里必须记录元组的具体信息，因为这个表是在RecordManager删除那些元组后返回的
//使用链表存储, 因为查询前并不知晓查询结果包含几个元组
class DeletedTuple {
public:
    void** values;
    DeletedTuple* Next;
    DeletedTuple(void) : values(0), Next(0) {}
    ~DeletedTuple(void) {}
};


//***********************************************************常用函数定义***********************************************************

/*
 * 生成空文件/文件夹.
 * Filename若以'/'结尾则代表创建文件夹, 否则代表创建空文件.
 * 若路径不存在也会强制生成.
 * 例如./Test/Test.txt, 如果没有./Test这个文件夹, 会先生成该文件夹, 然后在文件夹内生成Test.txt
 */
inline void Touch(string Filename) {
    int Last;
    for (Last = Filename.length() - 1; Last >= 0 && Filename[Last] != '/'; Last--);
    if (Last == -1) {//整个字符串内都没有'/', 直接创建空文件
        fstream fp;
        fp.open(Filename, ios::out);
        fp.close();
    }
    else {//先创建文件夹
        system((string("md \"") + Filename.substr(0, Last) + "\"").c_str());
        if (Last < Filename.length() - 1) {//如果Buffer不是以'/'结尾, 说明还要创建空文件
            fstream fp;
            fp.open(Filename, ios::out);
            fp.close();
        }
    }
}


/*
 * 删除指定文件或文件夹.
 * 如果Filename以'/'结尾, 则代表删除文件夹, 以及文件夹内所有内容.
 * 否则, 仅删除对应文件.
 */
inline void Remove(string Filename) {
    int Last;
    char Buffer[1024];
    for (int i = 0; i < Filename.length(); i++)
        if (Filename[i] == '/') Buffer[i] = '\\';
        else Buffer[i] = Filename[i];
    Buffer[Filename.length()] = '\0';
    for (Last = Filename.length() - 1; Last >= 0 && Filename[Last] != '/'; Last--);
    if (Last == -1 || Last != Filename.length() - 1)//整个字符串内都没有'/', 或字符串不以'/'结尾, 直接删除文件
        system(("del \"" + string(Buffer) + "\"").c_str());
    else //删除文件夹以及文件夹内所有内容
        system(("rd /s/q \"" + Filename + "\"").c_str());
}

/*
 * 若该文件不存在, 或文件大小不是块的整数倍, 则返回-1
 * 否则，返回该文件在磁盘中占用的总块数
 * 实际上就是文件内容大小/块大小
 */
inline int FileSize(string Filename) {
    fstream fp;
    fp.open(Filename, ios::in | ios::binary);
    if (fp.fail())
        return -1;
    fp.seekg(0, ios::end);
    if (fp.tellg() % BlockSize) {
        fp.close();
        return -1;
    }
    int ans = fp.tellg() / BlockSize;
    fp.close();
    return ans;
}

/*
 * 删除目录Path下所有名称为Filename的文件
 */
static void DeleteFiles(string Path, string Filename) {
    //文件句柄
    intptr_t  hFile = 0;
    //文件信息
    struct _finddata_t fileinfo;
    string p;
    if ((hFile = _findfirst(p.assign(Path).append("/*").c_str(), &fileinfo)) != -1) {
        do {
            //如果是目录, 递归
            //如果不是, 则判断是否是目标文件, 然后删除
            if ((fileinfo.attrib & _A_SUBDIR)) {
                if (strcmp(fileinfo.name, ".") != 0 && strcmp(fileinfo.name, "..") != 0)
                    DeleteFiles(p.assign(Path).append("/").append(fileinfo.name), Filename);
            }
            else if (fileinfo.name == Filename) {
                Remove(p.assign(Path).append("/").append(fileinfo.name));
            }
        } while (_findnext(hFile, &fileinfo) == 0);
        _findclose(hFile);
    }
}

/*
 * 判断目录Path下是否有名称为Filename的文件
 */
static bool FilesExist(string Path, string Filename) {
    //文件句柄
    intptr_t  hFile = 0;
    //文件信息
    struct _finddata_t fileinfo;
    string p;
    if ((hFile = _findfirst(p.assign(Path).append("/*").c_str(), &fileinfo)) != -1) {
        do {
            //如果是目录, 递归
            //如果不是, 则判断是否是目标文件, 然后删除
            if ((fileinfo.attrib & _A_SUBDIR)) {
                if (strcmp(fileinfo.name, ".") != 0 && strcmp(fileinfo.name, "..") != 0 && FilesExist(p.assign(Path).append("/").append(fileinfo.name), Filename))
                    return true;
            }
            else if (fileinfo.name == Filename) {
                return true; //名为Filename的文件存在
            }
        } while (_findnext(hFile, &fileinfo) == 0);
        _findclose(hFile);
    }
    return false;
}

/*
 * 比较两个值的大小, 值的类型由Type确定.
 * SQL_INT: 会把void*解释成int*;
 * SQL_FLOAT: 会把void*解释成float*;
 * SQL_CHAR: 会把void*解释成string*.
 * 返回值: 大于: 1; 等于: 0; 小于: -1.
 */
inline int Compare(const void* p1, const void* p2, unsigned char Type) {
    if (p1 == NULL || p2 == NULL)
        return -1;
    if (Type == SQL_INT) {
        if (*((int*)p1) < *((int*)p2))
            return -1;
        else if (*((int*)p1) == *((int*)p2))
            return 0;
        else
            return 1;
    }
    else if (Type == SQL_FLOAT) {
        if (*((float*)p1) < *((float*)p2))
            return -1;
        else if (*((float*)p1) == *((float*)p2))
            return 0;
        else
            return 1;
    }
    else {
        if (*((string*)p1) < *((string*)p2))
            return -1;
        else if (*((string*)p1) == *((string*)p2))
            return 0;
        else
            return 1;
    }
}

inline bool ValidFloat(string s) {
    int start = 0, l = s.size(), i;
    for (i = 0; s[i] == '+' || s[i] == '-'; i++);
    start = i;
    for (i = start; s[i] && s[i] >= '0' && s[i] <= '9'; i++);//找到除符号位以外, 第一个非数字字符
    if (!s[i]) return true;//是一个不带小数点的整数, 返回true
    if (s[i] != '.') return false;//不是数字字符也不是小数点, 报错
    start = i + 1;
    for (i = start; s[i]; i++)
        if (s[i] < '0' || s[i] > '9')
            return false;//小数点后面只能有数字, 否则报错
    return true;
}

inline bool ValidInt(string s) {
    int i, start;
    for (i = 0; s[i] == '+' || s[i] == '-'; i++);
    start = i;
    for (; s[i]; i++)
        if (s[i] < '0' || s[i]>'9')
            return false;
    //注意, 还要判断一种特殊情况, 即s只包含'+' 或'-'
    if (i == start) return false;
    return true;
}