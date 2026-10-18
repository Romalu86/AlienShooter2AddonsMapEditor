#pragma once

#include "basetype.h"
#include <new>

namespace DialogTreeOperations
{
template<class Node>
void RotateLeft(Node*& head, Node* current, Node* const& nilOwner)
{
    Node* const pivot = current->RightLink();
    current->RightLink() = pivot->LeftLink();
    Node* const child = pivot->LeftLink();
    if (child != nilOwner)
        child->ParentLink() = current;
    pivot->ParentLink() = current->ParentLink();
    Node* const header = head;
    if (current == header->ParentLink())
        header->ParentLink() = pivot;
    else
    {
        Node* const parent = current->ParentLink();
        if (current == parent->LeftLink())
            parent->LeftLink() = pivot;
        else
            parent->RightLink() = pivot;
    }
    pivot->LeftLink() = current;
    current->ParentLink() = pivot;
}

template<class Node>
void RotateRight(Node*& head, Node* current, Node* const& nilOwner)
{
    Node* const pivot = current->LeftLink();
    current->LeftLink() = pivot->RightLink();
    Node* const child = pivot->RightLink();
    if (child != nilOwner)
        child->ParentLink() = current;
    pivot->ParentLink() = current->ParentLink();
    Node* const header = head;
    if (current == header->ParentLink())
        header->ParentLink() = pivot;
    else
    {
        Node* const parent = current->ParentLink();
        if (current == parent->RightLink())
            parent->RightLink() = pivot;
        else
            parent->LeftLink() = pivot;
    }
    pivot->RightLink() = current;
    current->ParentLink() = pivot;
}

template<class Node>
Node* Successor(Node* current, const Node* sentinel)
{
    Node* const right = current->RightLink();
    if (right != sentinel)
    {
        current = right;
        while (current->LeftLink() != sentinel)
            current = current->LeftLink();
        return current;
    }
    Node* parent = current->ParentLink();
    while (current == parent->RightLink())
    {
        current = parent;
        parent = parent->ParentLink();
    }
    if (current->RightLink() != parent)
        current = parent;
    return current;
}

template<class Node>
Node* Minimum(Node* current, const Node* sentinel)
{
    while (current->LeftLink() != sentinel)
        current = current->LeftLink();
    return current;
}

template<class Node>
Node* Maximum(Node* current, const Node* sentinel)
{
    while (current->RightLink() != sentinel)
        current = current->RightLink();
    return current;
}

template<class Node>
Node* Predecessor(Node* current, Node* const& nilOwner)
{
    if (current->ColorValue() == 0u &&
        current->ParentLink()->ParentLink() == current)
        return current->RightLink();
    Node* const left = current->LeftLink();
    Node* const sentinel = nilOwner;
    if (left != sentinel)
    {
        current = left;
        while (current->RightLink() != sentinel)
            current = current->RightLink();
        return current;
    }
    Node* parent = current->ParentLink();
    while (current == parent->LeftLink())
    {
        current = parent;
        parent = parent->ParentLink();
    }
    return parent;
}

template<class Node, class ConstructValue, class LessThanParent>
Node* Insert(Node*& head, unsigned int& size, Node* hintNode, Node* parent,
    Node* const& nilOwner, ConstructValue constructValue, LessThanParent lessThanParent)
{
    Node* const node = new Node;
    node->ParentLink() = parent;
    node->ColorValue() = 0u;
    node->LeftLink() = nilOwner;
    node->RightLink() = nilOwner;
    constructValue(node);

    Node* header = head;
    ++size;
    const bool putLeft = parent == header || hintNode != nilOwner || lessThanParent(parent);
    if (putLeft)
    {
        parent->LeftLink() = node;
        header = head;
        if (parent == header)
        {
            header->ParentLink() = node;
            head->RightLink() = node;
        }
        else if (parent == header->LeftLink())
            header->LeftLink() = node;
    }
    else
    {
        parent->RightLink() = node;
        header = head;
        if (parent == header->RightLink()) header->RightLink() = node;
    }
    Node* current = node;
    while (current != head->ParentLink() &&
          current->ParentLink()->ColorValue() == 0u)
    {
        Node* parentNode = current->ParentLink();
        Node* grandparentNode = parentNode->ParentLink();
        if (parentNode == grandparentNode->LeftLink())
        {
            Node* uncle = grandparentNode->RightLink();
            if (uncle->ColorValue() == 0u)
            {
                parentNode->ColorValue() = 1u;
                uncle->ColorValue() = 1u;
                current->ParentLink()->ParentLink()->ColorValue() = 0u;
                current = current->ParentLink()->ParentLink();
            }
            else
            {
                if (current == parentNode->RightLink())
                {
                    current = parentNode;
                    RotateLeft(head, current, nilOwner);
                }
                current->ParentLink()->ColorValue() = 1u;
                current->ParentLink()->ParentLink()->ColorValue() = 0u;
                grandparentNode = current->ParentLink()->ParentLink();
                RotateRight(head, grandparentNode, nilOwner);
            }
        }
        else
        {
            Node* uncle = grandparentNode->LeftLink();
            if (uncle->ColorValue() == 0u)
            {
                parentNode->ColorValue() = 1u;
                uncle->ColorValue() = 1u;
                current->ParentLink()->ParentLink()->ColorValue() = 0u;
                current = current->ParentLink()->ParentLink();
            }
            else
            {
                if (current == parentNode->LeftLink())
                {
                    current = parentNode;
                    RotateRight(head, current, nilOwner);
                }
                current->ParentLink()->ColorValue() = 1u;
                current->ParentLink()->ParentLink()->ColorValue() = 0u;
                grandparentNode = current->ParentLink()->ParentLink();
                RotateLeft(head, grandparentNode, nilOwner);
            }
        }
    }
    head->ParentLink()->ColorValue() = 1u;
    return node;
}

}
