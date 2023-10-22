#include <iostream> 
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <mutex>
#include <fstream>

#define STORE_FILE "store/dumpFile"

std::mutex mtx;     // mutex for critical section
std::string delimiter = ":";

//跳表节点的结构
template<typename K, typename V> 
class Node {

public:
    
    Node() {} 

    Node(K k, V v, int); 

    ~Node();

    K get_key() const;

    V get_value() const;

    void set_value(V);
    
    // 用于存储指向下一层节点的指针的数组
    Node<K, V> **forward;

    int node_level;//节点的层数

private:
    K key;
    V value;
};

template<typename K, typename V> 
Node<K, V>::Node(const K k, const V v, int level) {//节点的构造函数
    this->key = k;
    this->value = v;
    this->node_level = level; 

    // level + 1, because array index is from 0 - level
    this->forward = new Node<K, V>*[level+1];//新建一个指针数组 里面可以存放level+1个node型指针 0 - level
    
	// Fill forward array with 0(NULL) 
    memset(this->forward, 0, sizeof(Node<K, V>*)*(level+1));
};

template<typename K, typename V> 
Node<K, V>::~Node() {
    delete []forward;
};

template<typename K, typename V> 
K Node<K, V>::get_key() const {
    return key;
};

template<typename K, typename V> 
V Node<K, V>::get_value() const {
    return value;
};
template<typename K, typename V> 
void Node<K, V>::set_value(V value) {
    this->value=value;
};

// 跳表类
template <typename K, typename V> 
class SkipList {

public: 
    SkipList(int);
    ~SkipList();
    int get_random_level();//随机获取层数
    Node<K, V>* create_node(K, V, int); //创造一个一个节点
    int insert_element(K, V);   //插入元素
    void display_list();        //展示跳表
    bool search_element(K);     //搜索K
    void delete_element(K);     //删除K
    void dump_file();
    void load_file();
    int size();

private:
    void get_key_value_from_string(const std::string& str, std::string* key, std::string* value);
    bool is_valid_string(const std::string& str);

private:    
    // 定义跳表的最大高度
    int _max_level;

    // 当前跳表的高度 
    int _skip_list_level;

    // 指向头结点的指针
    Node<K, V> *_header;

    //文件操作流
    std::ofstream _file_writer;
    std::ifstream _file_reader;

    // 当前跳表中的元素数
    int _element_count;
};

// 新建节点 层数是level
template<typename K, typename V>
Node<K, V>* SkipList<K, V>::create_node(const K k, const V v, int level) {
    Node<K, V> *n = new Node<K, V>(k, v, level);
    return n;
}

// Insert given key and value in skip list 
// return 1 means element exists  
// return 0 means insert successfully
/* 
                           +------------+
                           |  insert 50 |
                           +------------+
level 4     +-->1+                                                      100
                 |
                 |                      insert +----+
level 3         1+-------->10+---------------> | 50 |          70       100
                                               |    |
                                               |    |
level 2         1          10         30       | 50 |          70       100
                                               |    |
                                               |    |
level 1         1    4     10         30       | 50 |          70       100
                                               |    |
                                               |    |
level 0         1    4   9 10         30   40  | 50 |  60      70       100
                                               +----+

*/
template<typename K, typename V>
int SkipList<K, V>::insert_element(const K key, const V value) {//插入节点
    
    mtx.lock();//加锁
    Node<K, V> *current = this->_header;

    // create update array and initialize it 
    // update 是一个数组，每一层所能走的最远处的节点的位置
    Node<K, V> *update[_max_level+1];
    memset(update, 0, sizeof(Node<K, V>*)*(_max_level+1));  

    // 从跳表的最高层开始
    for(int i = _skip_list_level; i >= 0; i--) {
        while(current->forward[i] != NULL && current->forward[i]->get_key() < key) {//如果当前层的下一个节点有且key值小于目标值直接移动到下一个节点
            current = current->forward[i]; 
        }
        update[i] = current;//
    }

    // 到达级别 0 并将指针转发到需要插入密钥的右节点
    current = current->forward[0];

    // 找到目标值
    if (current != NULL && current->get_key() == key) {
        std::cout << "key: " << key << ", exists" << std::endl;
        mtx.unlock();
        return 1;//返回1说明节点已经存在
    }

    // 如果当前世NULL说明到达了最后一个节点
    // 如果没到达最后一个节点且没找到相等的数值需要新建一个节点插入到update节点和当前节点之间 update[0] and current node 
    if (current == NULL || current->get_key() != key ) {
        
        // 给节点生成一个随机高度 每生长一层有50%的概率
        int random_level = get_random_level();

        // 如果随机级别大于跳过列表的当前级别，则使用指向头部的指针初始化更新值 也就是用头指针初始化高度大于目前高度的那部分指针
        if (random_level > _skip_list_level) {
            for (int i = _skip_list_level + 1; i < random_level + 1; i++) {
                update[i] = _header;
            }
            _skip_list_level = random_level;
        }

        // 使用生成的随机高度新建一个节点 
        Node<K, V>* inserted_node = create_node(key, value, random_level);
        
        // 插入节点
        for (int i = 0; i <= random_level; i++) {
            inserted_node->forward[i] = update[i]->forward[i];// 使用update存储的值初始化新建的节点 指向下一个节点
            update[i]->forward[i] = inserted_node;//前一个节点指向新建节点
        }
        std::cout << "Successfully inserted key:" << key << ", value:" << value << std::endl;
        _element_count ++;
    }
    mtx.unlock();
    return 0;
}

// 展示出跳表的样子
template<typename K, typename V> 
void SkipList<K, V>::display_list() {

    std::cout << "\n*****Skip List*****"<<"\n"; 
    for (int i = 0; i <= _skip_list_level; i++) {
        Node<K, V> *node = this->_header->forward[i]; 
        std::cout << "Level " << i << ": ";
        while (node != NULL) {
            std::cout << node->get_key() << ":" << node->get_value() << ";";
            node = node->forward[i];
        }
        std::cout << std::endl;
    }
}

// 将数据存储到磁盘上 
template<typename K, typename V> 
void SkipList<K, V>::dump_file() {

    std::cout << "dump_file-----------------" << std::endl;
    _file_writer.open(STORE_FILE);
    Node<K, V> *node = this->_header->forward[0]; 

    //只存最后一层的数据
    while (node != NULL) {
        _file_writer << node->get_key() << ":" << node->get_value() << "\n";
        std::cout << node->get_key() << ":" << node->get_value() << ";\n";
        node = node->forward[0];
    }

    _file_writer.flush();
    _file_writer.close();
    return ;
}

// 将数据从磁盘上加载到redis数据库中
template<typename K, typename V> 
void SkipList<K, V>::load_file() {

    _file_reader.open(STORE_FILE);
    std::cout << "load_file-----------------" << std::endl;
    std::string line;
    std::string* key = new std::string();
    std::string* value = new std::string();
    while (getline(_file_reader, line)) {//一次处理一行 一行存一对数据
        get_key_value_from_string(line, key, value);
        if (key->empty() || value->empty()) {
            continue;
        }
        insert_element(*key, *value);//插入到跳表中
        std::cout << "key:" << *key << "value:" << *value << std::endl;
    }
    _file_reader.close();
}

// 获得当前跳表存的元素数 
template<typename K, typename V> 
int SkipList<K, V>::size() { 
    return _element_count;
}

template<typename K, typename V>
void SkipList<K, V>::get_key_value_from_string(const std::string& str, std::string* key, std::string* value) {

    if(!is_valid_string(str)) {
        return;
    }
    *key = str.substr(0, str.find(delimiter));
    *value = str.substr(str.find(delimiter)+1, str.length());
}

//判断是否为有效键值对
template<typename K, typename V>
bool SkipList<K, V>::is_valid_string(const std::string& str) {

    if (str.empty()) {
        return false;
    }
    if (str.find(delimiter) == std::string::npos) {
        return false;
    }
    return true;
}

// 从跳表中删除一个元素
template<typename K, typename V> 
void SkipList<K, V>::delete_element(K key) {

    mtx.lock();
    Node<K, V> *current = this->_header; 
    Node<K, V> *update[_max_level+1];
    memset(update, 0, sizeof(Node<K, V>*)*(_max_level+1));

    // start from highest level of skip list
    for (int i = _skip_list_level; i >= 0; i--) {
        while (current->forward[i] !=NULL && current->forward[i]->get_key() < key) {//找到目标节点的左边节点
            current = current->forward[i];
        }
        update[i] = current;
    }

    current = current->forward[0];
    if (current != NULL && current->get_key() == key) {//找到目标节点
       
        // 从最底层开始更新跳表
        for (int i = 0; i <= _skip_list_level; i++) {

            // 如果下一层节点不是当前节点说明已经删除到位了 可以跳出循环
            if (update[i]->forward[i] != current) 
                break;

            update[i]->forward[i] = current->forward[i];
        }

        // Remove levels which have no elements
        while (_skip_list_level > 0 && _header->forward[_skip_list_level] == 0) {//从当前的最高层判断 如果最高层没节点了要高度减一
            _skip_list_level --; 
        }

        std::cout << "Successfully deleted key "<< key << std::endl;
        _element_count --;
    }
    mtx.unlock();
    return;
}

// 查询一个元素
/*
                           +------------+
                           |  select 60 |
                           +------------+
level 4     +-->1+                                                      100
                 |
                 |
level 3         1+-------->10+------------------>50+           70       100
                                                   |
                                                   |
level 2         1          10         30         50|           70       100
                                                   |
                                                   |
level 1         1    4     10         30         50|           70       100
                                                   |
                                                   |
level 0         1    4   9 10         30   40    50+-->60      70       100
*/
template<typename K, typename V> 
bool SkipList<K, V>::search_element(K key) {

    std::cout << "search_element-----------------" << std::endl;
    Node<K, V> *current = _header;

    // start from highest level of skip list
    for (int i = _skip_list_level; i >= 0; i--) {
        while (current->forward[i] && current->forward[i]->get_key() < key) {
            current = current->forward[i];
        }
    }

    //reached level 0 and advance pointer to right node, which we search
    current = current->forward[0];

    // if current node have key equal to searched key, we get it
    if (current and current->get_key() == key) {
        std::cout << "Found key: " << key << ", value: " << current->get_value() << std::endl;
        return true;
    }

    std::cout << "Not Found Key:" << key << std::endl;
    return false;
}

// 跳表的构造函数
template<typename K, typename V> 
SkipList<K, V>::SkipList(int max_level) {

    this->_max_level = max_level;
    this->_skip_list_level = 0;
    this->_element_count = 0;

    // create header node and initialize key and value to null
    K k;
    V v;
    this->_header = new Node<K, V>(k, v, _max_level);//头结点是不存数据的且层数是最高层的高度
};

//跳表的析构函数
template<typename K, typename V> 
SkipList<K, V>::~SkipList() {

    if (_file_writer.is_open()) {
        _file_writer.close();
    }
    if (_file_reader.is_open()) {
        _file_reader.close();
    }
    delete _header;
}

//获得随机的高度
template<typename K, typename V>
int SkipList<K, V>::get_random_level(){

    int k = 1;
    while (rand() % 2) {//每次都有50%的几率高度增1
        k++;
    }
    k = (k < _max_level) ? k : _max_level;
    return k;
};
// vim: et tw=100 ts=4 sw=4 cc=120
