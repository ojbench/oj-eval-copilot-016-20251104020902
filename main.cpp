#include <iostream>
#include <fstream>
#include <cstring>
#include <string>
#include <algorithm>
#include <vector>

using namespace std;

const int MAX_KEY_LEN = 64;
const int ORDER = 100;

struct Entry {
    char key[MAX_KEY_LEN + 1];
    int value;
    
    Entry() : value(0) {
        memset(key, 0, sizeof(key));
    }
    
    Entry(const char* k, int v) : value(v) {
        memset(key, 0, sizeof(key));
        strncpy(key, k, MAX_KEY_LEN);
    }
    
    bool operator<(const Entry& other) const {
        int cmp = strcmp(key, other.key);
        if (cmp != 0) return cmp < 0;
        return value < other.value;
    }
    
    bool operator==(const Entry& other) const {
        return strcmp(key, other.key) == 0 && value == other.value;
    }
};

class BPlusTree {
private:
    struct Node {
        bool is_leaf;
        int num_keys;
        Entry entries[ORDER];
        long children[ORDER + 1];
        long next_leaf;
        
        Node() : is_leaf(true), num_keys(0), next_leaf(-1) {
            memset(children, -1, sizeof(children));
        }
    };
    
    fstream file;
    string filename;
    long root_pos;
    long free_pos;
    
    long allocate_node() {
        long pos = free_pos;
        free_pos += sizeof(Node);
        return pos;
    }
    
    void write_node(long pos, const Node& node) {
        file.seekp(pos);
        file.write(reinterpret_cast<const char*>(&node), sizeof(Node));
        file.flush();
    }
    
    void read_node(long pos, Node& node) {
        file.seekg(pos);
        file.read(reinterpret_cast<char*>(&node), sizeof(Node));
    }
    
    int find_index_in_node(const Node& node, const Entry& entry) {
        int i = 0;
        while (i < node.num_keys && entry < node.entries[i]) {
            i++;
        }
        return i;
    }
    
    void split_child(Node& parent, int index, long child_pos) {
        Node child, new_node;
        read_node(child_pos, child);
        
        new_node.is_leaf = child.is_leaf;
        int mid = ORDER / 2;
        
        if (child.is_leaf) {
            new_node.num_keys = child.num_keys - mid;
            for (int i = 0; i < new_node.num_keys; i++) {
                new_node.entries[i] = child.entries[mid + i];
            }
            child.num_keys = mid;
            
            new_node.next_leaf = child.next_leaf;
            long new_pos = allocate_node();
            child.next_leaf = new_pos;
            
            write_node(child_pos, child);
            write_node(new_pos, new_node);
            
            for (int i = parent.num_keys; i > index; i--) {
                parent.entries[i] = parent.entries[i - 1];
                parent.children[i + 1] = parent.children[i];
            }
            parent.entries[index] = new_node.entries[0];
            parent.children[index + 1] = new_pos;
            parent.num_keys++;
        } else {
            new_node.num_keys = child.num_keys - mid - 1;
            for (int i = 0; i < new_node.num_keys; i++) {
                new_node.entries[i] = child.entries[mid + 1 + i];
                new_node.children[i] = child.children[mid + 1 + i];
            }
            new_node.children[new_node.num_keys] = child.children[child.num_keys];
            
            Entry promote = child.entries[mid];
            child.num_keys = mid;
            
            long new_pos = allocate_node();
            write_node(child_pos, child);
            write_node(new_pos, new_node);
            
            for (int i = parent.num_keys; i > index; i--) {
                parent.entries[i] = parent.entries[i - 1];
                parent.children[i + 1] = parent.children[i];
            }
            parent.entries[index] = promote;
            parent.children[index + 1] = new_pos;
            parent.num_keys++;
        }
    }
    
    void insert_non_full(long pos, const Entry& entry) {
        Node node;
        read_node(pos, node);
        
        if (node.is_leaf) {
            int i = node.num_keys - 1;
            while (i >= 0 && entry < node.entries[i]) {
                node.entries[i + 1] = node.entries[i];
                i--;
            }
            node.entries[i + 1] = entry;
            node.num_keys++;
            write_node(pos, node);
        } else {
            int i = node.num_keys - 1;
            while (i >= 0 && entry < node.entries[i]) {
                i--;
            }
            i++;
            
            Node child;
            read_node(node.children[i], child);
            
            if (child.num_keys == ORDER) {
                split_child(node, i, node.children[i]);
                write_node(pos, node);
                
                read_node(pos, node);
                if (node.entries[i] < entry) {
                    i++;
                }
            }
            
            insert_non_full(node.children[i], entry);
        }
    }
    
    void delete_entry(long pos, const Entry& entry) {
        Node node;
        read_node(pos, node);
        
        if (node.is_leaf) {
            int i = 0;
            while (i < node.num_keys && node.entries[i] < entry) {
                i++;
            }
            
            if (i < node.num_keys && node.entries[i] == entry) {
                for (int j = i; j < node.num_keys - 1; j++) {
                    node.entries[j] = node.entries[j + 1];
                }
                node.num_keys--;
                write_node(pos, node);
            }
        } else {
            int i = 0;
            while (i < node.num_keys && node.entries[i] < entry) {
                i++;
            }
            
            if (node.children[i] != -1) {
                delete_entry(node.children[i], entry);
            }
        }
    }
    
    long find_start_leaf(long pos, const char* key) {
        Node node;
        read_node(pos, node);
        
        if (node.is_leaf) {
            return pos;
        }
        
        // Find the appropriate child to descend
        int i = 0;
        while (i < node.num_keys && strcmp(key, node.entries[i].key) >= 0) {
            i++;
        }
        
        return find_start_leaf(node.children[i], key);
    }
    
    void find_all(const char* key, vector<int>& values) {
        // Find the leaf that might contain the key
        long leaf_pos = find_start_leaf(root_pos, key);
        
        // Traverse leaf nodes until we no longer find the key
        while (leaf_pos != -1) {
            Node leaf;
            read_node(leaf_pos, leaf);
            
            bool found_in_this_leaf = false;
            for (int i = 0; i < leaf.num_keys; i++) {
                int cmp = strcmp(leaf.entries[i].key, key);
                if (cmp == 0) {
                    values.push_back(leaf.entries[i].value);
                    found_in_this_leaf = true;
                } else if (cmp > 0) {
                    // Keys are sorted, so we can stop
                    return;
                }
            }
            
            // If we didn't find the key in this leaf and it's after all entries, stop
            if (!found_in_this_leaf && leaf.num_keys > 0 && 
                strcmp(leaf.entries[leaf.num_keys - 1].key, key) < 0) {
                return;
            }
            
            leaf_pos = leaf.next_leaf;
        }
    }
    
public:
    BPlusTree(const string& fname) : filename(fname), root_pos(0), free_pos(sizeof(Node)) {
        file.open(filename, ios::in | ios::out | ios::binary);
        
        if (!file.is_open()) {
            file.clear();
            file.open(filename, ios::out | ios::binary);
            file.close();
            file.open(filename, ios::in | ios::out | ios::binary);
            
            Node root;
            write_node(root_pos, root);
        }
    }
    
    ~BPlusTree() {
        if (file.is_open()) {
            file.close();
        }
    }
    
    void insert(const char* key, int value) {
        Entry entry(key, value);
        
        Node root;
        read_node(root_pos, root);
        
        if (root.num_keys == ORDER) {
            Node new_root;
            new_root.is_leaf = false;
            new_root.num_keys = 0;
            long old_root_pos = allocate_node();
            
            write_node(old_root_pos, root);
            new_root.children[0] = old_root_pos;
            
            split_child(new_root, 0, old_root_pos);
            write_node(root_pos, new_root);
        }
        
        insert_non_full(root_pos, entry);
    }
    
    void remove(const char* key, int value) {
        Entry entry(key, value);
        delete_entry(root_pos, entry);
    }
    
    vector<int> find(const char* key) {
        vector<int> values;
        find_all(key, values);
        sort(values.begin(), values.end());
        return values;
    }
};

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    
    int n;
    cin >> n;
    
    BPlusTree tree("data.db");
    
    for (int i = 0; i < n; i++) {
        string cmd;
        cin >> cmd;
        
        if (cmd == "insert") {
            string key;
            int value;
            cin >> key >> value;
            tree.insert(key.c_str(), value);
        } else if (cmd == "delete") {
            string key;
            int value;
            cin >> key >> value;
            tree.remove(key.c_str(), value);
        } else if (cmd == "find") {
            string key;
            cin >> key;
            vector<int> values = tree.find(key.c_str());
            
            if (values.empty()) {
                cout << "null\n";
            } else {
                for (size_t j = 0; j < values.size(); j++) {
                    if (j > 0) cout << " ";
                    cout << values[j];
                }
                cout << "\n";
            }
        }
    }
    
    return 0;
}
