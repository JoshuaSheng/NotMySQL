//
// Created by Josh on 2024-01-02.
//

#include "collection.h"

std::runtime_error writeInsideReadTxErr {"Tried to write inside of a read transaction"};

Collection::Collection(std::vector<BYTE> name, pgnum root, Tx *tx): name(name), root(root), tx(tx) {
}

Item *Collection::find(vector<BYTE> key) {
    Node *node = tx->getNode(root);
    auto [containingNode, index, _] = node->findKey(key);
    if (index == -1) {
        return nullptr;
    }
    return containingNode->items[index];
}

void Collection::put(vector<BYTE> key, vector<BYTE> value) {
    if (!tx->write) {
        throw writeInsideReadTxErr;
    }

    Item *item = newItem(key, value);
    Node* rootNode;
    if (root == 0) {
        rootNode = tx->writeNode(tx->newNode(vector<Item *>{item}, vector<pgnum>{}));
        root = rootNode->pageNum;
        return;
    }
    else {
        rootNode = tx->getNode(root);
    }

    auto [targetNode, insertionIndex, ancestorsIndexes] = rootNode->findKey(key, false);

    if (targetNode->items.size() > insertionIndex && cmp_bytes(targetNode->items[insertionIndex]->key, key) == 0) {
        targetNode->items[insertionIndex]->value = value;
    } else {
        targetNode->addItem(item, insertionIndex);
    }
    tx->writeNode(targetNode);
    auto ancestors = getNodes(ancestorsIndexes);

    //rebalances nodes from the bottom up, except for root
    for (int i{static_cast<int>(ancestors.size() - 2)}; i >= 0; --i) {
        Node *currNode{ancestors[i + 1]};
        Node *parentNode{ancestors[i]};
        int currIndex{ancestorsIndexes[i+1]};
        if (currNode->isOverpopulated()) parentNode->split(currNode, currIndex);
    }
    //is rootNode being assigned to itself here?
    rootNode = ancestors[0];
    if (rootNode->isOverpopulated()) {
        Node *newRoot = tx->newNode(vector<Item *>{}, vector<pgnum> {rootNode->pageNum});
        newRoot->split(rootNode, 0);

        newRoot = tx->writeNode(newRoot);
        root = newRoot->pageNum;
    }
}

vector<Node *> Collection::getNodes(vector<int> indexes) {
    Node *rootNode = tx->getNode(root);
    vector<Node *>nodes {rootNode};
    Node *currNode{rootNode};
    for (int i{1}; i < indexes.size(); i++) {
        currNode = tx->getNode(currNode->childNodes[indexes[i]]);
        nodes.push_back(currNode);
    }
    return nodes;
}

void Collection::remove(std::vector<BYTE> key) {
    if (!tx->write) {
        throw writeInsideReadTxErr;
    }

    Node *rootNode = tx->getNode(root);
    auto [nodeToRemoveFrom, removeItemIndex, ancestorsIndexes] = rootNode->findKey(key);
    if (removeItemIndex == -1) {
        cout << "Couldn't find key to remove" << endl;
        return;
    }

    if (nodeToRemoveFrom->isLeaf()) {
        nodeToRemoveFrom->removeItemFromLeaf(removeItemIndex);
    }
    else {
        std::vector<int>affectedNodes = nodeToRemoveFrom->removeItemFromBranch(removeItemIndex);
        ancestorsIndexes.insert(ancestorsIndexes.end(), affectedNodes.begin(), affectedNodes.end());
    }
    std::vector<Node *>ancestors = getNodes(ancestorsIndexes);
    for (int i{static_cast<int>(ancestors.size() - 2)}; i >= 0; --i) {
        Node *parent = ancestors[i];
        Node *node = ancestors[i+1];
        if (node->isUnderpopulated()) {
            parent->rebalanceRemove(node, ancestorsIndexes[i+1]);
        }
    }

    rootNode = ancestors[0];
    if (rootNode->items.empty() && !rootNode->childNodes.empty()) {
        root = ancestors[1]->pageNum;
    }
}