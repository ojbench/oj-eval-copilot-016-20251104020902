#include <iostream>
#include <fstream>
#include <cstring>
#include <string>
#include <algorithm>
#include <vector>

using namespace std;

const int MAX_KEY_LEN = 64;
const int M = 50; // Reduced order for better disk performance

struct Pair {
    char key[MAX_KEY_LEN + 1];
    int value;
    
    Pair() : value(0) {
        memset(key, 0, sizeof(key));
    }
    
    Pair(const string& k, int v) : value(v) {
        memset(key, 0, sizeof(key));
        strncpy(key, k.c_str(), MAX_KEY_LEN);
    }
    
    bool operator<(const Pair& other) const {
        int cmp = strcmp(key, other.key);
        if (cmp != 0) return cmp < 0;
        return value < other.value;
    }
    
    bool operator==(const Pair& other) const {
        return strcmp(key, other.key) == 0 && value == other.value;
    }
};

class BPlusTree {
private:
    struct Node {
        bool isLeaf;
        int n; // number of keys
        Pair pairs[M];
        int children[M + 1];
        int next; // for leaf nodes
        
        Node() : isLeaf(true), n(0), next(-1) {
            for (int i = 0; i <= M; i++) children[i] = -1;
        }
    };
    
    fstream file;
    int rootPos;
    int freePos;
    
    void readNode(int pos, Node& node) {
        file.seekg(pos);
        file.read((char*)&node, sizeof(Node));
    }
    
    void writeNode(int pos, Node& node) {
        file.seekp(pos);
        file.write((char*)&node, sizeof(Node));
        file.flush();
    }
    
    int allocNode() {
        int pos = freePos;
        freePos += sizeof(Node);
        return pos;
    }
    
    void splitChild(int parentPos, int idx) {
        Node parent, child, newChild;
        readNode(parentPos, parent);
        readNode(parent.children[idx], child);
        
        newChild.isLeaf = child.isLeaf;
        int mid = M / 2;
        
        if (child.isLeaf) {
            // For leaf: keep mid in left, move rest to right
            newChild.n = child.n - mid;
            for (int i = 0; i < newChild.n; i++) {
                newChild.pairs[i] = child.pairs[mid + i];
            }
            child.n = mid;
            
            // Update linked list
            newChild.next = child.next;
            int newChildPos = allocNode();
            child.next = newChildPos;
            
            writeNode(parent.children[idx], child);
            writeNode(newChildPos, newChild);
            
            // Insert first key of newChild into parent
            for (int i = parent.n; i > idx; i--) {
                parent.pairs[i] = parent.pairs[i - 1];
                parent.children[i + 1] = parent.children[i];
            }
            parent.pairs[idx] = newChild.pairs[0];
            parent.children[idx + 1] = newChildPos;
            parent.n++;
            
            writeNode(parentPos, parent);
        } else {
            // For internal node: promote middle key
            newChild.n = child.n - mid - 1;
            for (int i = 0; i < newChild.n; i++) {
                newChild.pairs[i] = child.pairs[mid + 1 + i];
                newChild.children[i] = child.children[mid + 1 + i];
            }
            newChild.children[newChild.n] = child.children[child.n];
            
            Pair promoteKey = child.pairs[mid];
            child.n = mid;
            
            int newChildPos = allocNode();
            writeNode(parent.children[idx], child);
            writeNode(newChildPos, newChild);
            
            // Insert promoted key into parent
            for (int i = parent.n; i > idx; i--) {
                parent.pairs[i] = parent.pairs[i - 1];
                parent.children[i + 1] = parent.children[i];
            }
            parent.pairs[idx] = promoteKey;
            parent.children[idx + 1] = newChildPos;
            parent.n++;
            
            writeNode(parentPos, parent);
        }
    }
    
    void insertNonFull(int pos, const Pair& pair) {
        Node node;
        readNode(pos, node);
        
        if (node.isLeaf) {
            int i = node.n - 1;
            while (i >= 0 && pair < node.pairs[i]) {
                node.pairs[i + 1] = node.pairs[i];
                i--;
            }
            node.pairs[i + 1] = pair;
            node.n++;
            writeNode(pos, node);
        } else {
            int i = node.n - 1;
            while (i >= 0 && pair < node.pairs[i]) {
                i--;
            }
            i++;
            
            Node child;
            readNode(node.children[i], child);
            if (child.n == M) {
                splitChild(pos, i);
                readNode(pos, node);
                if (node.pairs[i] < pair) {
                    i++;
                }
            }
            insertNonFull(node.children[i], pair);
        }
    }
    
public:
    BPlusTree(const string& filename) : rootPos(0), freePos(sizeof(Node)) {
        file.open(filename, ios::in | ios::out | ios::binary);
        if (!file.is_open()) {
            file.clear();
            file.open(filename, ios::out | ios::binary);
            file.close();
            file.open(filename, ios::in | ios::out | ios::binary);
            
            Node root;
            writeNode(rootPos, root);
        }
    }
    
    ~BPlusTree() {
        if (file.is_open()) file.close();
    }
    
    void insert(const string& key, int value) {
        Pair pair(key, value);
        
        Node root;
        readNode(rootPos, root);
        
        if (root.n == M) {
            Node newRoot;
            newRoot.isLeaf = false;
            newRoot.n = 0;
            int oldRootPos = allocNode();
            writeNode(oldRootPos, root);
            newRoot.children[0] = oldRootPos;
            
            splitChild(rootPos, 0);
            writeNode(rootPos, newRoot);
        }
        
        insertNonFull(rootPos, pair);
    }
    
    void remove(const string& key, int value) {
        Pair target(key, value);
        removeHelper(rootPos, target);
    }
    
    void removeHelper(int pos, const Pair& pair) {
        Node node;
        readNode(pos, node);
        
        if (node.isLeaf) {
            int i = 0;
            while (i < node.n && node.pairs[i] < pair) i++;
            if (i < node.n && node.pairs[i] == pair) {
                for (int j = i; j < node.n - 1; j++) {
                    node.pairs[j] = node.pairs[j + 1];
                }
                node.n--;
                writeNode(pos, node);
            }
        } else {
            int i = 0;
            while (i < node.n && node.pairs[i] < pair) i++;
            if (node.children[i] != -1) {
                removeHelper(node.children[i], pair);
            }
        }
    }
    
    vector<int> find(const string& key) {
        vector<int> result;
        
        // Find starting leaf
        int pos = rootPos;
        while (true) {
            Node node;
            readNode(pos, node);
            
            if (node.isLeaf) {
                // Scan this and subsequent leaves
                int visitCount = 0;
                const int MAX_VISITS = 100000; // Safety limit
                
                while (pos != -1 && visitCount < MAX_VISITS) {
                    readNode(pos, node);
                    
                    bool foundInThisLeaf = false;
                    for (int i = 0; i < node.n; i++) {
                        int cmp = strcmp(node.pairs[i].key, key.c_str());
                        if (cmp == 0) {
                            result.push_back(node.pairs[i].value);
                            foundInThisLeaf = true;
                        } else if (cmp > 0) {
                            goto done;
                        }
                    }
                    
                    // Only continue if we might find more
                    if (node.n > 0 && strcmp(node.pairs[node.n - 1].key, key.c_str()) < 0) {
                        break;
                    }
                    
                    pos = node.next;
                    visitCount++;
                }
                done:
                break;
            }
            
            // Navigate to child
            int i = 0;
            while (i < node.n && strcmp(node.pairs[i].key, key.c_str()) <= 0) {
                i++;
            }
            pos = node.children[i];
        }
        
        sort(result.begin(), result.end());
        return result;
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
            tree.insert(key, value);
        } else if (cmd == "delete") {
            string key;
            int value;
            cin >> key >> value;
            tree.remove(key, value);
        } else if (cmd == "find") {
            string key;
            cin >> key;
            vector<int> values = tree.find(key);
            
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
