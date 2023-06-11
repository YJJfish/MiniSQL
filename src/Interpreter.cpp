#include <iostream>

#include "Global.h"
#include "Exception.h"
#include "Interpreter.h"
#include <regex>
#include <ctime>

using namespace std;
clock_t t_start, t_end;

bool Interpreter::ExecFile(string FileName)
{
    ifstream iFile; //读入文件流
    iFile.open(FileName);
    if (iFile.fail())
        throw InvalidFile(FileName);
    while (!iFile.eof()) {
        string Command = "";//Command是一条SQL语句
        iFile >> Command;  //读取文件的一个字符串
        if (Command == "")return true;
        while (Command[Command.length() - 1] != ';') { //一个语句以';'结束
            if (iFile.eof()) return false; //语句语法错误,末尾没有';'
            string Line = "";
            iFile >> Line;
            Command += " " + Line;
        }
        cout << "Executing: " << Command << endl;
        ExecCommand(Command);
        if (!ReturnValue.State) return false;
    }
    return true;
}


const ReturnInfo& Interpreter::ExecCommand(string s)
{
    ReturnValue.State = 1;
    t_start = clock(); //开始计时
    try {   //异常抛出检查
        /*------------------------------regex define-------------------------------*/
        regex QUIT("quit;");
        regex EXECFILE("execfile");
        regex CREATE("create");
        regex DROP("drop");
        regex INSERT("insert");
        regex SELECT("select");
        regex DELETE("delete");
        regex TABLE("table");
        regex INDEX("index");
        regex FROM("from");
        regex WHERE("where");
        regex AND("and");
        regex INT("int");
        regex FLOAT("float");
        regex CHAR("char");
        regex UNIQUE("unique");
        regex PRIMARY("primary");
        regex ill_char("[^0-9a-zA-Z*#<=>._ ]");    //检查非法字符
        regex TableRegex, AttrRegex, FileRegex, ConRegex, OpRegex, ValueRegex, IndexRegex, before;
        regex KeyRegex, PrimaryRegex;
        smatch result, TableName, AttrName, FILENAME, Key, PrimaryKey;
        string temp = s;

        /*------------------------check illegal character-------------------------*/
        //if (regex_search(temp, result, ill_char))
            //throw SyntaxError();

        /*--------------------------------parsing---------------------------------*/

        //quit
        if (regex_search(temp, result, QUIT)) {
            ReturnValue.Quit = 1;
            ReturnValue.Message = "";
        }

        //execfile
        else if (regex_search(temp, result, EXECFILE)) {
            FileRegex = "[0-9a-zA-Z_.]+(?=;)";
            if (regex_search(temp, FILENAME, FileRegex)) {
                string FileName = FILENAME[0].str();
                if (!ExecFile(FileName))
                    ReturnValue.State = 0;
            }
            else throw SyntaxError(); //如果没有匹配到文件名，返回语法错误
        }

        //select
        else if (regex_search(temp, result, SELECT)) {
            TableRegex = "[0-9a-zA-Z_]+(?= where)"; //匹配表名
            ConRegex = "[a-zA-Z<=>.'0-9_]+(?= and)"; //匹配条件
            OpRegex = "[<=>]+"; //匹配操作符
            AttrRegex = "[0-9a-zA-Z.'_]+(?=[<>=])";  //匹配属性名
            ValueRegex = "[a-zA-Z.'0-9_]+(?=;)";   //匹配属性值
            smatch Condition, Operator, Value;
            string table_name;
            if (regex_search(temp, TableName, TableRegex)) { //如果select有条件
                table_name = TableName[0].str();
                before = ";";    //把最后一个条件后的';'替换成' and'，便于匹配
                temp = regex_replace(temp,before," and");
            }
            else{
                TableRegex = "[0-9a-zA-Z_]+(?=;)";
                if(regex_search(temp,TableName,TableRegex))
                    table_name = TableName[0].str();
            }
            if (cm.TableExists(TableName[0].str())) {
                TableSchema table;
                cm.GetTable(table_name, table);   //读取表的模式
                int length = 0; //条件的数量
                string::const_iterator start = temp.begin();
                string::const_iterator end = temp.end();
                while (regex_search(start, end, Condition, ConRegex)) {
                    length++;
                    start = Condition[0].second;
                }
                string AttrNames[1024];
                int Ops[1024];
                void* Values[1024];
                start = temp.begin();
                end = temp.end();
                int ConIndex = 0; //条件下标
                while (regex_search(start, end, Condition, ConRegex)) {
                    string condition = Condition[0].str(); //转换条件匹配结果，以进一步匹配Attr, Op和Value
                    condition += ";"; //条件末尾补上';'以
                    if (regex_search(condition, Operator, OpRegex)) { //---------------------------------------匹配条件里的操作符
                        string op_string = Operator[0].str();
                        if (op_string == "=")       Ops[ConIndex] = 0;
                        else if (op_string == "<>") Ops[ConIndex] = 1;
                        else if (op_string == "<")  Ops[ConIndex] = 2;
                        else if (op_string == ">")  Ops[ConIndex] = 3;
                        else if (op_string == "<=") Ops[ConIndex] = 4;
                        else if (op_string == ">=") Ops[ConIndex] = 5;
                        if (regex_search(condition, AttrName, AttrRegex)) { //---------------------------------匹配条件里的属性名
                            bool isAttr = false;    //检查where子句的条件是否是该表的列属性
                            int AttrIndex; //列属性的下标
                            for (AttrIndex = 0; AttrIndex < table.NumAttributes; AttrIndex++)
                                if (table.AttributeName[AttrIndex] == AttrName[0].str()) {
                                    isAttr = true;
                                    break;
                                }
                            if (!isAttr) throw NonexistentAttr(table_name, AttrName[0].str());
                            AttrNames[ConIndex] = AttrName[0].str();
                            if (regex_search(condition, Value, ValueRegex)) { //------------------------------匹配条件里的属性值
                                if (table.Type[AttrIndex] == SQL_INT) {
                                    int* P_int = new(int);
                                    const string int_value = Value[0].str();
                                    if (ValidInt(int_value)) {
                                        *P_int = stoi(int_value);
                                        Values[ConIndex] = (void*)P_int;
                                    }
                                    else //属性值与属性的类型不匹配
                                        throw InvalidValue(table.AttributeName[AttrIndex], Value[0].str());
                                }
                                else if (table.Type[AttrIndex] == SQL_FLOAT) {
                                    float* P_float = new(float);
                                    const string float_value = Value[0].str();
                                    if (ValidFloat(float_value)) {
                                        *P_float = stof(float_value);
                                        Values[ConIndex] = (void*)P_float;
                                    }
                                    else //属性值与属性的类型不匹配
                                        throw InvalidValue(table.AttributeName[AttrIndex], Value[0].str());
                                }
                                else if (table.Type[AttrIndex] == SQL_CHAR) {
                                    string char_value = Value[0].str();
                                    if (char_value[0] == '\'' &&
                                        char_value[char_value.length() - 1] == '\''
                                        && char_value.length() <= table.Length[AttrIndex] + 2) {
                                        char_value.erase(0, 1);  //去除字符串的''
                                        char_value.erase(char_value.length() - 1);
                                        while (char_value.length() < table.Length[AttrIndex])
                                            char_value.append(" ");
                                        string* P_char = new(string);
                                        *P_char = char_value;
                                        Values[ConIndex] = (void*)P_char;
                                    }
                                    else //属性值与属性的类型不匹配
                                        throw InvalidValue(table.AttributeName[AttrIndex], Value[0].str());
                                }
                            }
                            else throw SyntaxError();
                        }
                        else throw SyntaxError();
                    }
                    else throw SyntaxError();
                    ConIndex++;
                    start = Condition[0].second;
                }
                ReturnValue.Counter++;
                int RecordNum = api.Select(ReturnValue.Counter, table, length, AttrNames, Ops, Values);
                for (int i = 0; i < ConIndex; i++) delete Values[i];
                ReturnValue.Message = to_string(RecordNum) + " row(s) in set";
            }
            else throw NonexistentTable(TableName[0].str());
        }

        //insert
        else if (regex_search(temp, result, INSERT)) {
            TableRegex = "[0-9a-zA-Z_]+(?= values)";  //匹配表名
            ValueRegex = "[0-9._a-zA-Z']+(?=,)"; //匹配值
            before = "\\)";    //把最后一个值后的')'替换成','，便于匹配
            temp = regex_replace(temp, before, ",");
            smatch Values;
            if (regex_search(temp, TableName, TableRegex)) {
                string table_name = TableName[0].str(); //转换表名为字符串
                if (cm.TableExists(TableName[0].str())) { //CatalogManager检查表是否存在
                    TableSchema table; //存储表的模式
                    cm.GetTable(table_name, table);   //读取表的模式
                    void* values[255];  //输入值转化为内部数据形式，void* []
                    int index = 0;
                    string::const_iterator start = temp.begin();
                    string::const_iterator end = temp.end();
                    while (regex_search(start, end, Values, ValueRegex)) {
                        if (table.Type[index] == SQL_INT) {
                            int* P_int = new(int);
                            const string int_value = Values[0].str();
                            if (ValidInt(int_value)) {
                                *P_int = stoi(int_value);
                                values[index] = (void*)P_int;
                            }
                            else //插入的值不匹配
                                throw InvalidValue(table.AttributeName[index], Values[0]);
                        }
                        else if (table.Type[index] == SQL_FLOAT) {
                            float* P_float = new(float);
                            const string float_value = Values[0].str();
                            if (ValidFloat(float_value)) {
                                *P_float = stof(float_value);
                                values[index] = (void*)P_float;
                            }
                            else //插入的值不匹配
                                throw InvalidValue(table.AttributeName[index], Values[0]);
                        }
                        else if (table.Type[index] == SQL_CHAR) {
                            string* P_char = new(string);
                            string char_value = Values[0].str();
                            if (char_value[0] == '\'' && char_value[char_value.length() - 1] == '\''
                                && char_value.length() <= table.Length[index] + 2) {
                                char_value.erase(0, 1);  //去除字符串的''
                                char_value.erase(char_value.length() - 1);
                                while (char_value.length() < table.Length[index])
                                    char_value.append(" ");
                                *P_char = char_value;
                                values[index] = (void*)P_char;
                            }
                            else //插入的值不匹配
                                throw InvalidValue(table.AttributeName[index], Values[0]);
                        }
                        index++;
                        start = Values[0].second;
                    }
                    if (index != table.NumAttributes) throw SyntaxError();//值的个数不对, 语法错误
                    api.Insert(table, values);
                    for (int i = 0; i < index; i++) delete values[i];//释放内存
                    ReturnValue.Message = "";
                }
                else throw NonexistentTable(table_name);
            }
            else throw SyntaxError(); //如果没有匹配到表名，返回语法错误
        }

        //drop table
        else if (regex_search(temp, result, DROP) && regex_search(temp, result, TABLE)) {
            TableRegex = "[0-9a-zA-Z_]+(?=;)"; //匹配表名
            if (regex_search(temp, TableName, TableRegex)) {
                string table_name = TableName[0].str(); //转换表名为字符串
                if (cm.TableExists(table_name)) { //CatalogManager检查表是否存在
                    api.DropTable(table_name);
                    ReturnValue.Message = "";
                }
                else throw NonexistentTable(table_name);
            }
            else throw SyntaxError(); //如果没有匹配到表名，返回语法错误
        }

        //drop index
        else if (regex_search(temp, result, DROP) && regex_search(temp, result, INDEX)) {
            IndexRegex = "[0-9a-zA-Z_]+(?=;)"; //匹配索引名
            smatch IndexName;
            if (regex_search(temp, IndexName, IndexRegex)) {
                string index_name = IndexName[0].str(); //转换索引名为字符串
                if (cm.IndexExists(index_name)) { //CatalogManager检查索引是否存在
                    api.DropIndex(index_name);
                    ReturnValue.Message = "";
                }
                else throw NonexistentIndex(index_name);
            }
            else throw SyntaxError(); //如果没有匹配到索引名，返回语法错误
        }

        //create table
        else if (regex_search(temp, result, CREATE) && regex_search(temp, result, TABLE)) {
            TableRegex = "[0-9a-zA-Z_]+(?=\\()";  //匹配表名
            KeyRegex = "[0-9 a-zA-Z_()]+(?=,)";  //匹配列
            PrimaryRegex = "[0-9 a-zA-Z_()]+(?=\\);)"; //匹配主键
            if (regex_search(temp, TableName, TableRegex)) {
                string table_name = TableName[0].str(); //转换表名为字符串
                if (!cm.TableExists(table_name)) {
                    TableSchema table;
                    table.TableName = table_name;
                    string::const_iterator start = temp.begin();
                    string::const_iterator end = temp.end();
                    while (regex_search(start, end, Key, KeyRegex)) { //遍历,得到表的列个数
                        start = Key[0].second;
                        table.NumAttributes++;
                    }
                    table.AttributeName = new string[table.NumAttributes]; //分配指针内存
                    table.Type = new unsigned char[table.NumAttributes];
                    table.Unique = new bool[table.NumAttributes];
                    table.Length = new unsigned char[table.NumAttributes];
                    start = temp.begin();
                    end = temp.end();
                    int AttrIndex = 0; //属性下标
                    while (regex_search(start, end, Key, KeyRegex)) { //遍历，填入表的列信息
                        string KeyResult = Key[0].str();    //转换一列的匹配结果以进一步匹配列名和类型名
                        if (regex_search(KeyResult, result, INT)) { //匹配列类型并填入
                            AttrRegex = "[0-9a-zA-Z_]+(?= int)";
                            table.Type[AttrIndex] = SQL_INT;
                        }
                        else if (regex_search(KeyResult, result, FLOAT)) {
                            AttrRegex = "[0-9a-zA-Z_]+(?= float)";
                            table.Type[AttrIndex] = SQL_FLOAT;
                        }
                        else if (regex_search(KeyResult, result, CHAR)) {
                            AttrRegex = "[0-9a-zA-Z_]+(?= char)";
                            table.Type[AttrIndex] = SQL_CHAR;
                            regex LenRegex("[0-9]+(?=\\))");
                            smatch LenSm;
                            if (regex_search(KeyResult, LenSm, LenRegex)) { //记录char的长度
                                string LenString = LenSm[0].str();
                                int Len = stoi(LenString);
                                table.Length[AttrIndex] = Len;
                            }
                        }
                        else throw SyntaxError(); //如果没有匹配到属性类型，返回语法错误
                        if (regex_search(KeyResult, AttrName, AttrRegex)) //填入列名
                            table.AttributeName[AttrIndex] = AttrName[0].str();
                        else throw SyntaxError(); //如果没有匹配到列名，返回语法错误
                        if (regex_search(KeyResult, result, UNIQUE)) //检查是否unique
                            table.Unique[AttrIndex] = 1;
                        else table.Unique[AttrIndex] = 0;
                        start = Key[0].second;
                        AttrIndex++;
                    }
                    if (regex_search(temp, PrimaryKey, PrimaryRegex)) {
                        AttrRegex = "[0-9a-zA-Z_]+(?=\\))";
                        string PrimaryResult = PrimaryKey[0].str(); //转换主键匹配结果，以进一步匹配主键名
                        if (regex_search(PrimaryResult, AttrName, AttrRegex)) { //匹配主键名
                            int isPrimary = 0;
                            for (unsigned char i = 0; i < table.NumAttributes; i++) {
                                if (table.AttributeName[i] == AttrName[0].str()) {
                                    isPrimary = 1;
                                    table.Primary = i;
                                    table.Unique[i] = 1;
                                    break;
                                }
                            }
                            if (!isPrimary) throw NonexistentAttr(TableName[0].str(), AttrName[0].str()); //如果主键名不在表中，返回语法错误
                        }
                        else throw SyntaxError(); //如果没有匹配到列名，返回语法错误
                    }
                    api.CreateTable(table);
                    ReturnValue.Message = "";
                }
                else throw DuplicatedTable(table_name);
            }
            else throw SyntaxError(); //如果没有匹配到表名，返回语法错误
        }

        //create index
        else if (regex_search(temp, result, CREATE) && regex_search(temp, result, INDEX)) {
            IndexRegex = "[0-9a-zA-Z_]+(?= on)";  //匹配索引名
            TableRegex = "[0-9a-zA-Z_]+(?=\\()";  //匹配表名
            AttrRegex = "[0-9a-zA-Z_]+(?=\\))";  //匹配索引名
            smatch IndexName;  //存储匹配结果
            if (regex_search(temp, IndexName, IndexRegex)) {
                string index_name = IndexName[0].str(); //转换索引名为字符串
                if (regex_search(temp, TableName, TableRegex)) {
                    string table_name = TableName[0].str(); //转换表名为字符串
                    if (regex_search(temp, AttrName, AttrRegex)) {
                        if (cm.IndexExists(index_name))  //检查索引是否存在
                            throw DuplicatedIndex(index_name);
                        if (!cm.TableExists(table_name))  //检查表是否存在
                            throw NonexistentTable(table_name);
                        TableSchema table;  //存储表的模式
                        IndexSchema index;  //新索引的实例化
                        index.TableName = TableName[0].str();
                        index.IndexName = IndexName[0].str();
                        cm.GetTable(table_name, table);   //读取表的模式
                        bool AttrExists = 0;    //检查属性是否存在
                        unsigned char AttributeNumber;    //属性编号,属性类型
                        for (AttributeNumber = 0; AttributeNumber < table.NumAttributes; AttributeNumber++) {
                            if (table.AttributeName[AttributeNumber] == AttrName[0].str()) {
                                if (!table.Unique[AttributeNumber])  //索引定义在非Unique属性上
                                    throw InvalidAttrForIndex(TableName[0].str(), AttrName[0].str());
                                AttrExists = 1;
                                index.AttributeNumber = AttributeNumber;
                                index.AttributeName = AttrName[0].str();
                                index.Type = table.Type[AttributeNumber];
                                index.Length = table.Length[AttributeNumber];
                                break;
                            }
                        }
                        if (!AttrExists) throw NonexistentAttr(table_name, AttrName[0].str());
                        api.CreateIndex(table, index);
                        ReturnValue.Message = "";
                    }
                    else throw SyntaxError();
                }
                else throw SyntaxError();
            }
            else throw SyntaxError();
        }

        //delete
        else if (regex_search(temp, result, DELETE)) {
            TableRegex = "[0-9a-zA-Z_]+(?= where)"; //匹配表名
            ConRegex = "[a-zA-Z<=>.'0-9_]+(?= and)"; //匹配条件
            OpRegex = "[<=>]+"; //匹配操作符
            AttrRegex = "[0-9a-zA-Z.'_]+(?=[<>=])";  //匹配属性名
            ValueRegex = "[a-zA-Z.'0-9_]+(?=;)";   //匹配属性值
            smatch Condition, Operator, Value; string table_name;
            if (regex_search(temp, TableName, TableRegex)) { //如果delete有条件
                table_name = TableName[0].str();
                before = ";";
                temp = regex_replace(temp, before, " and");
            }
            else {
                TableRegex = "[0-9a-zA-Z_]+(?=;)";
                if (regex_search(temp, TableName, TableRegex))
                    table_name = TableName[0].str();
                else throw SyntaxError();
            }
            if (cm.TableExists(table_name)) {
                TableSchema table;
                cm.GetTable(table_name, table);   //读取表的模式
                int length = 0; //条件的数量
                string::const_iterator start = temp.begin();
                string::const_iterator end = temp.end();
                while (regex_search(start, end, Condition, ConRegex)) {
                    length++;
                    start = Condition[0].second;
                }
                string AttrNames[1024];
                int Ops[1024];
                void* Values[1024];
                start = temp.begin();
                end = temp.end();
                int ConIndex = 0; //条件下标
                while (regex_search(start, end, Condition, ConRegex)) {
                    string condition = Condition[0].str(); //转换条件匹配结果，以进一步匹配Attr, Op和Value
                    condition += ";"; //条件末尾补上';'以
                    if (regex_search(condition, Operator, OpRegex)) { //---------------------------------------匹配条件里的操作符
                        string op_string = Operator[0].str();
                        if (op_string == "=")       Ops[ConIndex] = 0;
                        else if (op_string == "<>") Ops[ConIndex] = 1;
                        else if (op_string == "<")  Ops[ConIndex] = 2;
                        else if (op_string == ">")  Ops[ConIndex] = 3;
                        else if (op_string == "<=") Ops[ConIndex] = 4;
                        else if (op_string == ">=") Ops[ConIndex] = 5;
                        if (regex_search(condition, AttrName, AttrRegex)) { //---------------------------------匹配条件里的属性名
                            bool isAttr = false;    //检查where子句的条件是否是该表的列属性
                            int AttrIndex; //列属性的下标
                            for (AttrIndex = 0; AttrIndex < table.NumAttributes; AttrIndex++)
                                if (table.AttributeName[AttrIndex] == AttrName[0].str()) {
                                    isAttr = true;
                                    break;
                                }
                            if (!isAttr) throw NonexistentAttr(table_name, AttrName[0].str());
                            AttrNames[ConIndex] = AttrName[0].str();
                            if (regex_search(condition, Value, ValueRegex)) { //------------------------------匹配条件里的属性值
                                if (table.Type[AttrIndex] == SQL_INT) {
                                    int* P_int = new(int);
                                    const string int_value = Value[0].str();
                                    if (ValidInt(int_value)) {
                                        *P_int = stoi(int_value);
                                        Values[ConIndex] = (void*)P_int;
                                    }
                                    else //属性值与属性的类型不匹配
                                        throw InvalidValue(table.AttributeName[AttrIndex], Value[0].str());
                                }
                                else if (table.Type[AttrIndex] == SQL_FLOAT) {
                                    float* P_float = new(float);
                                    const string float_value = Value[0].str();
                                    if (ValidFloat(float_value)) {
                                        *P_float = stof(float_value);
                                        Values[ConIndex] = (void*)P_float;
                                    }
                                    else //属性值与属性的类型不匹配
                                        throw InvalidValue(table.AttributeName[AttrIndex], Value[0].str());
                                }
                                else if (table.Type[AttrIndex] == SQL_CHAR) {
                                    string char_value = Value[0].str();
                                    if (char_value[0] == '\'' &&
                                        char_value[char_value.length() - 1] == '\''
                                        && char_value.length() <= table.Length[AttrIndex] + 2) {
                                        char_value.erase(0, 1);  //去除字符串的''
                                        char_value.erase(char_value.length() - 1);
                                        while (char_value.length() < table.Length[AttrIndex])
                                            char_value.append(" ");
                                        string* P_char = new(string);
                                        *P_char = char_value;
                                        Values[ConIndex] = (void*)P_char;
                                    }
                                    else //属性值与属性的类型不匹配
                                        throw InvalidValue(table.AttributeName[AttrIndex], Value[0].str());
                                }
                            }
                            else throw SyntaxError();
                        }
                        else throw SyntaxError();
                    }
                    else throw SyntaxError();
                    ConIndex++;
                    start = Condition[0].second;
                }
                int RecordNum = api.Delete(table, length, AttrNames, Ops, Values);
                for (int i = 0; i < ConIndex; i++) delete Values[i];
                ReturnValue.Message = to_string(RecordNum) + " row(s) affected";
            }
            else throw NonexistentTable(TableName[0].str());
        }

        //不属于SQL语句
        else throw SyntaxError();

    }
    catch (exception& e) {
        ReturnValue.State = 0;
        ReturnValue.Message = e.what();
    }

    t_end = clock();    //结束计时
    ReturnValue.Time = t_end - t_start;
    //double runtime = (double)(t_end - t_start)/CLOCKS_PER_SEC;
    //string ExecTime(" (" + to_string(runtime) + "sec)");
    //ReturnValue.Message += ExecTime;
    return ReturnValue;
}
