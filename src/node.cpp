//
// Created by Josh on 2023-09-17.
//

#include <cstring>
#include "node.h"

bool Node::isLeaf() {
    return childNodes.empty();
}

Node * newEmptyNode() {
    return new Node{};
}

Item * newItem(std::vector<BYTE> key, std::vector<BYTE> value) {
    return new Item{key, value};
}

std::vector<BYTE> *Node::serialize(std::vector<BYTE> &buf) {
    int leftPos{0};
    size_t rightPos{buf.size() - 1};
    uint8_t bitSetVar = this->isLeaf() ? 1 : 0;
    buf[0] = bitSetVar;
    leftPos += sizeof(bitSetVar);
    uint16_t itemLength{uint16_t(this->items.size())};
    memcpy(buf.data() + leftPos, &itemLength, sizeof(uint16_t));
    leftPos += sizeof(uint16_t);
    for (int i{0}; i < items.size(); ++i) {
        Item *item {items[i]};
        if (!isLeaf()) {
            pgnum childNode {childNodes[i]};
            memcpy(buf.data() + leftPos, &childNode, sizeof(pgnum));
            leftPos += pageNumSize;
        }

        int8_t klen {static_cast<int8_t>(item->key.size())};
        int8_t vlen {static_cast<int8_t>(item->value.size())};

        uint16_t offset = rightPos - klen - vlen - 2;
        memcpy(buf.data() + leftPos, &offset, sizeof(offset));
        leftPos += sizeof(offset);

        rightPos -= vlen;
        memcpy(buf.data() + rightPos, &item->value, vlen);
        rightPos -= sizeof(vlen);
        memcpy(buf.data() + rightPos, &vlen, sizeof(vlen));
        rightPos -= klen;
        memcpy(buf.data() + rightPos, &item->key, klen);
        rightPos -= sizeof(klen);
        memcpy(buf.data() + rightPos, &klen, sizeof(klen));


    }

    if (!isLeaf()) {
        pgnum childNode {childNodes.back()};
        memcpy(buf.data() + leftPos, &childNode, sizeof(pgnum));
    }

    return &buf;
}

void Node::deserialize(std::vector<BYTE> &buf) {
    int leftPos{0};
    int isLeaf{int(buf[0])};
    uint16_t itemsCount{};
    leftPos += sizeof(uint8_t);
    memcpy(&itemsCount, buf.data() + leftPos, sizeof(itemsCount));
    leftPos += sizeof(uint8_t) + sizeof(uint16_t);

    for (int i{0}; i < itemsCount; ++i) {
        if (!isLeaf) {
            pgnum newPageNum{};
            memcpy(&newPageNum, buf.data() + leftPos, pageNumSize);
            leftPos += pageNumSize;
            childNodes.push_back(newPageNum);
        }

        uint16_t offset{};
        memcpy(&offset, buf.data() + leftPos, sizeof(uint16_t));
        leftPos += sizeof(uint16_t);

        //read key, value from offset
        int8_t klen {};
        memcpy(&klen, buf.data() + offset, sizeof(int8_t));
        offset += sizeof(int8_t);

        std::vector<BYTE> key{buf.data() + offset, buf.data() + offset + klen - 1};
        offset += klen;

        int8_t vlen {};
        memcpy(&vlen, buf.data() + offset, sizeof(int8_t));
        offset += sizeof(int8_t);

        std::vector<BYTE> value{buf.data() + offset, buf.data() + offset + vlen - 1};
        offset += vlen;

        items.push_back(new Item{key, value});
    }

    if (!isLeaf) {
        pgnum newPageNum{};
        memcpy(&newPageNum, buf.data() + leftPos, pageNumSize);
        childNodes.push_back(newPageNum);
    }
}

Node *Node::writeNode(Node *node) {
    return dal->writeNode(node);
}

void Node::writeNodes(std::vector<Node *> nodes) {
    for (auto node: nodes) {
        writeNode(node);
    }
}

Node *Node::getNode(pgnum pageNum) {
    return dal->getNode(pageNum);
}

//returns -1 if l < r, 0 if l == r, and 1 otherwise
int cmp_bytes(std::vector<unsigned char> &l, std::vector<unsigned char> &r) {
    if (l.size() < r.size()) {
        return -1;
    }
    else if (l.size() > r.size()) {
        return 1;
    }
    else {
        return strncmp(reinterpret_cast<char *>(l.data()), reinterpret_cast<char *>(r.data()), l.size());
    }
}

std::pair<bool, int> Node::findKeyInNode(std::vector<BYTE> key) {
    for (int i{0}; i < items.size(); ++i) {
        int res = cmp_bytes(items[i]->key, key);
        if (res == 0) {
            return std::make_pair(true, i);
        }
        else if (res == 1) {
            return std::make_pair(false, i);
        }
    }
    return std::make_pair(false, items.size());
}

std::pair<Node *, int> findKeyHelper(Node *node, std::vector<BYTE> &key) {
    auto [wasFound, index] {node->findKeyInNode(key)};
    if (wasFound) {
        return std::make_pair(node, index);
    }

    if (node->isLeaf()) {
        return std::make_pair(nullptr, -1);
    }

    Node * nextChild = node->getNode(node->childNodes[index]);
    return findKeyHelper(nextChild, key);
}

std::pair<Node *, int> Node::findKey(std::vector<BYTE> key) {
    return findKeyHelper(this, key);
}


