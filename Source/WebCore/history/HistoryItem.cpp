/*
 * Copyright (C) 2005, 2006, 2008, 2011, 2014 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "HistoryItem.h"

#include "CachedPage.h"
#include "Document.h"
#include "IconDatabase.h"
#include "KeyedCoding.h"
#include "PageCache.h"
#include "ResourceRequest.h"
#include "SerializedScriptValue.h"
#include "SharedBuffer.h"
#include <stdio.h>
#include <wtf/CurrentTime.h>
#include <wtf/DateMath.h>
#include <wtf/Decoder.h>
#include <wtf/Encoder.h>
#include <wtf/text/CString.h>

namespace WebCore {

const uint32_t backForwardTreeEncodingVersion = 2;

static long long generateSequenceNumber()
{
    // Initialize to the current time to reduce the likelihood of generating
    // identifiers that overlap with those from past/future browser sessions.
    static long long next = static_cast<long long>(currentTime() * 1000000.0);
    return ++next;
}

static void defaultNotifyHistoryItemChanged(HistoryItem*)
{
}

void (*notifyHistoryItemChanged)(HistoryItem*) = defaultNotifyHistoryItemChanged;

HistoryItem::HistoryItem()
    : m_pageScaleFactor(0)
    , m_lastVisitWasFailure(false)
    , m_isTargetItem(false)
    , m_itemSequenceNumber(generateSequenceNumber())
    , m_documentSequenceNumber(generateSequenceNumber())
    , m_next(0)
    , m_prev(0)
#if PLATFORM(IOS)
    , m_scale(0)
    , m_scaleIsInitial(false)
    , m_bookmarkID(0)
#endif
{
}

HistoryItem::HistoryItem(const String& urlString, const String& title)
    : m_urlString(urlString)
    , m_originalURLString(urlString)
    , m_title(title)
    , m_pageScaleFactor(0)
    , m_lastVisitWasFailure(false)
    , m_isTargetItem(false)
    , m_itemSequenceNumber(generateSequenceNumber())
    , m_documentSequenceNumber(generateSequenceNumber())
    , m_next(0)
    , m_prev(0)
#if PLATFORM(IOS)
    , m_scale(0)
    , m_scaleIsInitial(false)
    , m_bookmarkID(0)
#endif
{    
    iconDatabase().retainIconForPageURL(m_urlString);
}

HistoryItem::HistoryItem(const String& urlString, const String& title, const String& alternateTitle)
    : m_urlString(urlString)
    , m_originalURLString(urlString)
    , m_title(title)
    , m_displayTitle(alternateTitle)
    , m_pageScaleFactor(0)
    , m_lastVisitWasFailure(false)
    , m_isTargetItem(false)
    , m_itemSequenceNumber(generateSequenceNumber())
    , m_documentSequenceNumber(generateSequenceNumber())
    , m_next(0)
    , m_prev(0)
#if PLATFORM(IOS)
    , m_scale(0)
    , m_scaleIsInitial(false)
    , m_bookmarkID(0)
#endif
{
    iconDatabase().retainIconForPageURL(m_urlString);
}

HistoryItem::HistoryItem(const URL& url, const String& target, const String& parent, const String& title)
    : m_urlString(url.string())
    , m_originalURLString(url.string())
    , m_target(target)
    , m_parent(parent)
    , m_title(title)
    , m_pageScaleFactor(0)
    , m_lastVisitWasFailure(false)
    , m_isTargetItem(false)
    , m_itemSequenceNumber(generateSequenceNumber())
    , m_documentSequenceNumber(generateSequenceNumber())
    , m_next(0)
    , m_prev(0)
#if PLATFORM(IOS)
    , m_scale(0)
    , m_scaleIsInitial(false)
    , m_bookmarkID(0)
#endif
{    
    iconDatabase().retainIconForPageURL(m_urlString);
}

HistoryItem::~HistoryItem()
{
    ASSERT(!m_cachedPage);
    iconDatabase().releaseIconForPageURL(m_urlString);
}

inline HistoryItem::HistoryItem(const HistoryItem& item)
    : RefCounted<HistoryItem>()
    , m_urlString(item.m_urlString)
    , m_originalURLString(item.m_originalURLString)
    , m_referrer(item.m_referrer)
    , m_target(item.m_target)
    , m_parent(item.m_parent)
    , m_title(item.m_title)
    , m_displayTitle(item.m_displayTitle)
    , m_scrollPoint(item.m_scrollPoint)
    , m_pageScaleFactor(item.m_pageScaleFactor)
    , m_lastVisitWasFailure(item.m_lastVisitWasFailure)
    , m_isTargetItem(item.m_isTargetItem)
    , m_itemSequenceNumber(item.m_itemSequenceNumber)
    , m_documentSequenceNumber(item.m_documentSequenceNumber)
    , m_formContentType(item.m_formContentType)
#if PLATFORM(IOS)
    , m_scale(item.m_scale)
    , m_scaleIsInitial(item.m_scaleIsInitial)
    , m_bookmarkID(item.m_bookmarkID)
    , m_sharedLinkUniqueIdentifier(item.m_sharedLinkUniqueIdentifier)
#endif
{
    if (item.m_formData)
        m_formData = item.m_formData->copy();
        
    unsigned size = item.m_children.size();
    m_children.reserveInitialCapacity(size);
    for (unsigned i = 0; i < size; ++i)
        m_children.uncheckedAppend(item.m_children[i]->copy());

    if (item.m_redirectURLs)
        m_redirectURLs = std::make_unique<Vector<String>>(*item.m_redirectURLs);
}

PassRefPtr<HistoryItem> HistoryItem::copy() const
{
    return adoptRef(new HistoryItem(*this));
}

void HistoryItem::reset()
{
    iconDatabase().releaseIconForPageURL(m_urlString);

    m_urlString = String();
    m_originalURLString = String();
    m_referrer = String();
    m_target = String();
    m_parent = String();
    m_title = String();
    m_displayTitle = String();

    m_lastVisitWasFailure = false;
    m_isTargetItem = false;

    m_redirectURLs = nullptr;

    m_itemSequenceNumber = generateSequenceNumber();

    m_stateObject = 0;
    m_documentSequenceNumber = generateSequenceNumber();

    m_formData = 0;
    m_formContentType = String();

    clearChildren();
}

const String& HistoryItem::urlString() const
{
    return m_urlString;
}

// The first URL we loaded to get to where this history item points.  Includes both client
// and server redirects.
const String& HistoryItem::originalURLString() const
{
    return m_originalURLString;
}

const String& HistoryItem::title() const
{
    return m_title;
}

const String& HistoryItem::alternateTitle() const
{
    return m_displayTitle;
}

bool HistoryItem::hasCachedPageExpired() const
{
    return m_cachedPage ? m_cachedPage->hasExpired() : false;
}

URL HistoryItem::url() const
{
    return URL(ParsedURLString, m_urlString);
}

URL HistoryItem::originalURL() const
{
    return URL(ParsedURLString, m_originalURLString);
}

const String& HistoryItem::referrer() const
{
    return m_referrer;
}

const String& HistoryItem::target() const
{
    return m_target;
}

const String& HistoryItem::parent() const
{
    return m_parent;
}

void HistoryItem::setAlternateTitle(const String& alternateTitle)
{
    m_displayTitle = alternateTitle;
    notifyHistoryItemChanged(this);
}

void HistoryItem::setURLString(const String& urlString)
{
    if (m_urlString != urlString) {
        iconDatabase().releaseIconForPageURL(m_urlString);
        m_urlString = urlString;
        iconDatabase().retainIconForPageURL(m_urlString);
    }
    
    notifyHistoryItemChanged(this);
}

void HistoryItem::setURL(const URL& url)
{
    pageCache()->remove(this);
    setURLString(url.string());
    clearDocumentState();
}

void HistoryItem::setOriginalURLString(const String& urlString)
{
    m_originalURLString = urlString;
    notifyHistoryItemChanged(this);
}

void HistoryItem::setReferrer(const String& referrer)
{
    m_referrer = referrer;
    notifyHistoryItemChanged(this);
}

void HistoryItem::setTitle(const String& title)
{
    m_title = title;
    notifyHistoryItemChanged(this);
}

void HistoryItem::setTarget(const String& target)
{
    m_target = target;
    notifyHistoryItemChanged(this);
}

void HistoryItem::setParent(const String& parent)
{
    m_parent = parent;
}

const IntPoint& HistoryItem::scrollPoint() const
{
    return m_scrollPoint;
}

void HistoryItem::setScrollPoint(const IntPoint& point)
{
    m_scrollPoint = point;
}

void HistoryItem::clearScrollPoint()
{
    m_scrollPoint.setX(0);
    m_scrollPoint.setY(0);
}

float HistoryItem::pageScaleFactor() const
{
    return m_pageScaleFactor;
}

void HistoryItem::setPageScaleFactor(float scaleFactor)
{
    m_pageScaleFactor = scaleFactor;
}

void HistoryItem::setDocumentState(const Vector<String>& state)
{
    m_documentState = state;
}

const Vector<String>& HistoryItem::documentState() const
{
    return m_documentState;
}

void HistoryItem::clearDocumentState()
{
    m_documentState.clear();
}

bool HistoryItem::isTargetItem() const
{
    return m_isTargetItem;
}

void HistoryItem::setIsTargetItem(bool flag)
{
    m_isTargetItem = flag;
}

void HistoryItem::setStateObject(PassRefPtr<SerializedScriptValue> object)
{
    m_stateObject = object;
}

void HistoryItem::addChildItem(PassRefPtr<HistoryItem> child)
{
    ASSERT(!childItemWithTarget(child->target()));
    m_children.append(child);
}

void HistoryItem::setChildItem(PassRefPtr<HistoryItem> child)
{
    ASSERT(!child->isTargetItem());
    unsigned size = m_children.size();
    for (unsigned i = 0; i < size; ++i)  {
        if (m_children[i]->target() == child->target()) {
            child->setIsTargetItem(m_children[i]->isTargetItem());
            m_children[i] = child;
            return;
        }
    }
    m_children.append(child);
}

HistoryItem* HistoryItem::childItemWithTarget(const String& target) const
{
    unsigned size = m_children.size();
    for (unsigned i = 0; i < size; ++i) {
        if (m_children[i]->target() == target)
            return m_children[i].get();
    }
    return 0;
}

HistoryItem* HistoryItem::childItemWithDocumentSequenceNumber(long long number) const
{
    unsigned size = m_children.size();
    for (unsigned i = 0; i < size; ++i) {
        if (m_children[i]->documentSequenceNumber() == number)
            return m_children[i].get();
    }
    return 0;
}

// <rdar://problem/4895849> HistoryItem::findTargetItem() should be replaced with a non-recursive method.
HistoryItem* HistoryItem::findTargetItem()
{
    if (m_isTargetItem)
        return this;
    unsigned size = m_children.size();
    for (unsigned i = 0; i < size; ++i) {
        if (HistoryItem* match = m_children[i]->targetItem())
            return match;
    }
    return 0;
}

HistoryItem* HistoryItem::targetItem()
{
    HistoryItem* foundItem = findTargetItem();
    return foundItem ? foundItem : this;
}

const HistoryItemVector& HistoryItem::children() const
{
    return m_children;
}

bool HistoryItem::hasChildren() const
{
    return !m_children.isEmpty();
}

void HistoryItem::clearChildren()
{
    m_children.clear();
}

bool HistoryItem::isAncestorOf(const HistoryItem* item) const
{
    for (size_t i = 0; i < m_children.size(); ++i) {
        HistoryItem* child = m_children[i].get();
        if (child == item)
            return true;
        if (child->isAncestorOf(item))
            return true;
    }
    return false;
}

// We do same-document navigation if going to a different item and if either of the following is true:
// - The other item corresponds to the same document (for history entries created via pushState or fragment changes).
// - The other item corresponds to the same set of documents, including frames (for history entries created via regular navigation)
bool HistoryItem::shouldDoSameDocumentNavigationTo(HistoryItem* otherItem) const
{
    if (this == otherItem)
        return false;

    if (stateObject() || otherItem->stateObject())
        return documentSequenceNumber() == otherItem->documentSequenceNumber();
    
    if ((url().hasFragmentIdentifier() || otherItem->url().hasFragmentIdentifier()) && equalIgnoringFragmentIdentifier(url(), otherItem->url()))
        return documentSequenceNumber() == otherItem->documentSequenceNumber();        
    
    return hasSameDocumentTree(otherItem);
}

// Does a recursive check that this item and its descendants have the same
// document sequence numbers as the other item.
bool HistoryItem::hasSameDocumentTree(HistoryItem* otherItem) const
{
    if (documentSequenceNumber() != otherItem->documentSequenceNumber())
        return false;
        
    if (children().size() != otherItem->children().size())
        return false;

    for (size_t i = 0; i < children().size(); i++) {
        HistoryItem* child = children()[i].get();
        HistoryItem* otherChild = otherItem->childItemWithDocumentSequenceNumber(child->documentSequenceNumber());
        if (!otherChild || !child->hasSameDocumentTree(otherChild))
            return false;
    }

    return true;
}

// Does a non-recursive check that this item and its immediate children have the
// same frames as the other item.
bool HistoryItem::hasSameFrames(HistoryItem* otherItem) const
{
    if (target() != otherItem->target())
        return false;
        
    if (children().size() != otherItem->children().size())
        return false;

    for (size_t i = 0; i < children().size(); i++) {
        if (!otherItem->childItemWithTarget(children()[i]->target()))
            return false;
    }

    return true;
}

String HistoryItem::formContentType() const
{
    return m_formContentType;
}

void HistoryItem::setFormInfoFromRequest(const ResourceRequest& request)
{
    m_referrer = request.httpReferrer();
    
    if (equalIgnoringCase(request.httpMethod(), "POST")) {
        // FIXME: Eventually we have to make this smart enough to handle the case where
        // we have a stream for the body to handle the "data interspersed with files" feature.
        m_formData = request.httpBody();
        m_formContentType = request.httpContentType();
    } else {
        m_formData = 0;
        m_formContentType = String();
    }
}

void HistoryItem::setFormData(PassRefPtr<FormData> formData)
{
    m_formData = formData;
}

void HistoryItem::setFormContentType(const String& formContentType)
{
    m_formContentType = formContentType;
}

FormData* HistoryItem::formData()
{
    return m_formData.get();
}

bool HistoryItem::isCurrentDocument(Document* doc) const
{
    // FIXME: We should find a better way to check if this is the current document.
    return equalIgnoringFragmentIdentifier(url(), doc->url());
}

void HistoryItem::addRedirectURL(const String& url)
{
    if (!m_redirectURLs)
        m_redirectURLs = std::make_unique<Vector<String>>();

    // Our API allows us to store all the URLs in the redirect chain, but for
    // now we only have a use for the final URL.
    (*m_redirectURLs).resize(1);
    (*m_redirectURLs)[0] = url;
}

Vector<String>* HistoryItem::redirectURLs() const
{
    return m_redirectURLs.get();
}

void HistoryItem::setRedirectURLs(std::unique_ptr<Vector<String>> redirectURLs)
{
    m_redirectURLs = std::move(redirectURLs);
}

void HistoryItem::encodeBackForwardTree(Encoder& encoder) const
{
    encoder.encodeUInt32(backForwardTreeEncodingVersion);

    encodeBackForwardTreeNode(encoder);
}

void HistoryItem::encodeBackForwardTree(KeyedEncoder& encoder) const
{
    encoder.encodeUInt32("version", backForwardTreeEncodingVersion);

    encoder.encodeObject("root", *this, [](KeyedEncoder& encoder, const HistoryItem& item) {
        item.encodeBackForwardTreeNode(encoder);
    });
}

#if PLATFORM(IOS)
static void encodeRect(Encoder& encoder, const FloatRect& floatRect)
{
    encoder.encodeFloat(floatRect.x());
    encoder.encodeFloat(floatRect.y());
    encoder.encodeFloat(floatRect.width());
    encoder.encodeFloat(floatRect.height());
}

static void encodeRect(Encoder& encoder, const IntRect& intRect)
{
    encoder.encodeInt32(intRect.x());
    encoder.encodeInt32(intRect.y());
    encoder.encodeInt32(intRect.width());
    encoder.encodeInt32(intRect.height());
}

static void encodeSize(Encoder& encoder, const FloatSize& floatSize)
{
    encoder.encodeFloat(floatSize.width());
    encoder.encodeFloat(floatSize.height());
}

static void encodeSize(Encoder& encoder, const IntSize& intSize)
{
    encoder.encodeInt32(intSize.width());
    encoder.encodeInt32(intSize.height());
}
#endif

void HistoryItem::encodeBackForwardTreeNode(Encoder& encoder) const
{
    size_t size = m_children.size();
    encoder.encodeUInt64(size);
    for (size_t i = 0; i < size; ++i) {
        const HistoryItem& child = *m_children[i];

        encoder.encodeString(child.m_originalURLString);

        encoder.encodeString(child.m_urlString);

        child.encodeBackForwardTreeNode(encoder);
    }

    encoder.encodeInt64(m_documentSequenceNumber);

    size = m_documentState.size();
    encoder.encodeUInt64(size);
    for (size_t i = 0; i < size; ++i)
        encoder.encodeString(m_documentState[i]);

    encoder.encodeString(m_formContentType);

    encoder.encodeBool(m_formData);
    if (m_formData)
        m_formData->encode(encoder);

    encoder.encodeInt64(m_itemSequenceNumber);

    encoder.encodeString(m_referrer);

    encoder.encodeInt32(m_scrollPoint.x());
    encoder.encodeInt32(m_scrollPoint.y());
    
    encoder.encodeFloat(m_pageScaleFactor);

    encoder.encodeBool(m_stateObject);
    if (m_stateObject)
        encoder.encodeBytes(m_stateObject->data().data(), m_stateObject->data().size());

    encoder.encodeString(m_target);

#if PLATFORM(IOS)
    encodeRect(encoder, m_exposedContentRect);
    encodeRect(encoder, m_unobscuredContentRect);
    encodeSize(encoder, m_minimumLayoutSizeInScrollViewCoordinates);
    encodeSize(encoder, m_contentSize);
    encoder.encodeBool(m_scaleIsInitial);
#endif
}

void HistoryItem::encodeBackForwardTreeNode(KeyedEncoder& encoder) const
{
    encoder.encodeObjects("children", m_children.begin(), m_children.end(), [](KeyedEncoder& encoder, const RefPtr<HistoryItem>& child) {
        encoder.encodeString("originalURLString", child->m_originalURLString);
        encoder.encodeString("urlString", child->m_urlString);

        child->encodeBackForwardTreeNode(encoder);
    });

    encoder.encodeInt64("documentSequenceNumber", m_documentSequenceNumber);

    encoder.encodeObjects("documentState", m_documentState.begin(), m_documentState.end(), [](KeyedEncoder& encoder, const String& string) {
        encoder.encodeString("string", string);
    });

    encoder.encodeString("formContentType", m_formContentType);

    encoder.encodeConditionalObject("formData", m_formData.get(), [](KeyedEncoder& encoder, const FormData& formData) {
        formData.encode(encoder);
    });

    encoder.encodeInt64("itemSequenceNumber", m_itemSequenceNumber);

    encoder.encodeString("referrer", m_referrer);

    encoder.encodeObject("scrollPoint", m_scrollPoint, [](KeyedEncoder& encoder, const IntPoint& scrollPoint) {
        encoder.encodeInt32("x", scrollPoint.x());
        encoder.encodeInt32("y", scrollPoint.y());
    });

    encoder.encodeFloat("pageScaleFactor", m_pageScaleFactor);

    encoder.encodeConditionalObject("stateObject", m_stateObject.get(), [](KeyedEncoder& encoder, const SerializedScriptValue& stateObject) {
        encoder.encodeBytes("data", stateObject.data().data(), stateObject.data().size());
    });

    encoder.encodeString("target", m_target);
}

struct DecodeRecursionStackElement {
    RefPtr<HistoryItem> node;
    size_t i;
    uint64_t size;

    DecodeRecursionStackElement(PassRefPtr<HistoryItem> node, size_t i, uint64_t size)
        : node(node)
        , i(i)
        , size(size)
    {
    }
};

#if PLATFORM(IOS)
static bool decodeRect(Decoder& decoder, FloatRect& floatRect)
{
    float x;
    if (!decoder.decodeFloat(x))
        return false;
    float y;
    if (!decoder.decodeFloat(y))
        return false;
    float width;
    if (!decoder.decodeFloat(width))
        return false;
    float height;
    if (!decoder.decodeFloat(height))
        return false;
    floatRect = FloatRect(x, y, width, height);
    return true;
}

static bool decodeRect(Decoder& decoder, IntRect& intRect)
{
    int x;
    if (!decoder.decodeInt32(x))
        return false;
    int y;
    if (!decoder.decodeInt32(y))
        return false;
    int width;
    if (!decoder.decodeInt32(width))
        return false;
    int height;
    if (!decoder.decodeInt32(height))
        return false;
    intRect = IntRect(x, y, width, height);
    return true;
}

static bool decodeSize(Decoder& decoder, FloatSize& floatSize)
{
    float width;
    if (!decoder.decodeFloat(width))
        return false;
    float height;
    if (!decoder.decodeFloat(height))
        return false;
    floatSize = FloatSize(width, height);
    return true;
}

static bool decodeSize(Decoder& decoder, IntSize& intSize)
{
    int width;
    if (!decoder.decodeInt32(width))
        return false;
    int height;
    if (!decoder.decodeInt32(height))
        return false;
    intSize = IntSize(width, height);
    return true;
}
#endif

PassRefPtr<HistoryItem> HistoryItem::decodeBackForwardTree(const String& topURLString, const String& topTitle, const String& topOriginalURLString, Decoder& decoder)
{
    // Since the data stream is not trusted, the decode has to be non-recursive.
    // We don't want bad data to cause a stack overflow.

    uint32_t version;
    if (!decoder.decodeUInt32(version))
        return 0;
    if (version != backForwardTreeEncodingVersion)
        return 0;

    String urlString = topURLString;
    String title = topTitle;
    String originalURLString = topOriginalURLString;

    Vector<DecodeRecursionStackElement, 16> recursionStack;

recurse:
    RefPtr<HistoryItem> node = create(urlString, title);

    node->setOriginalURLString(originalURLString);

    title = String();

    uint64_t size;
    if (!decoder.decodeUInt64(size))
        return 0;
    size_t i;
    RefPtr<HistoryItem> child;
    for (i = 0; i < size; ++i) {
        if (!decoder.decodeString(originalURLString))
            return 0;

        if (!decoder.decodeString(urlString))
            return 0;

        recursionStack.append(DecodeRecursionStackElement(node.release(), i, size));
        goto recurse;

resume:
        node->m_children.append(child.release());
    }

    if (!decoder.decodeInt64(node->m_documentSequenceNumber))
        return 0;

    if (!decoder.decodeUInt64(size))
        return 0;
    for (i = 0; i < size; ++i) {
        String state;
        if (!decoder.decodeString(state))
            return 0;
        node->m_documentState.append(state);
    }

    if (!decoder.decodeString(node->m_formContentType))
        return 0;

    bool hasFormData;
    if (!decoder.decodeBool(hasFormData))
        return 0;
    if (hasFormData) {
        node->m_formData = FormData::decode(decoder);
        if (!node->m_formData)
            return 0;
    }

    if (!decoder.decodeInt64(node->m_itemSequenceNumber))
        return 0;

    if (!decoder.decodeString(node->m_referrer))
        return 0;

    int32_t x;
    if (!decoder.decodeInt32(x))
        return 0;
    int32_t y;
    if (!decoder.decodeInt32(y))
        return 0;
    node->m_scrollPoint = IntPoint(x, y);
    
    if (!decoder.decodeFloat(node->m_pageScaleFactor))
        return 0;

    bool hasStateObject;
    if (!decoder.decodeBool(hasStateObject))
        return 0;
    if (hasStateObject) {
        Vector<uint8_t> bytes;
        if (!decoder.decodeBytes(bytes))
            return 0;
        node->m_stateObject = SerializedScriptValue::adopt(bytes);
    }

    if (!decoder.decodeString(node->m_target))
        return 0;

#if PLATFORM(IOS)
    if (!decodeRect(decoder, node->m_exposedContentRect))
        return nullptr;
    if (!decodeRect(decoder, node->m_unobscuredContentRect))
        return nullptr;
    if (!decodeSize(decoder, node->m_minimumLayoutSizeInScrollViewCoordinates))
        return nullptr;
    if (!decodeSize(decoder, node->m_contentSize))
        return nullptr;
    if (!decoder.decodeBool(node->m_scaleIsInitial))
        return nullptr;
#endif

    // Simulate recursion with our own stack.
    if (!recursionStack.isEmpty()) {
        DecodeRecursionStackElement& element = recursionStack.last();
        child = node.release();
        node = element.node.release();
        i = element.i;
        size = element.size;
        recursionStack.removeLast();
        goto resume;
    }

    return node.release();
}

PassRefPtr<HistoryItem> HistoryItem::decodeBackForwardTree(const String&, const String&, const String&, KeyedDecoder& decoder)
{
    uint32_t version;
    if (!decoder.decodeUInt32("version", version))
        return nullptr;
    
    if (version != backForwardTreeEncodingVersion)
        return nullptr;

    // FIXME: Implement.
    return nullptr;
}

#ifndef NDEBUG

int HistoryItem::showTree() const
{
    return showTreeWithIndent(0);
}

int HistoryItem::showTreeWithIndent(unsigned indentLevel) const
{
    Vector<char> prefix;
    for (unsigned i = 0; i < indentLevel; ++i)
        prefix.append("  ", 2);
    prefix.append("\0", 1);

    fprintf(stderr, "%s+-%s (%p)\n", prefix.data(), m_urlString.utf8().data(), this);
    
    int totalSubItems = 0;
    for (unsigned i = 0; i < m_children.size(); ++i)
        totalSubItems += m_children[i]->showTreeWithIndent(indentLevel + 1);
    return totalSubItems + 1;
}

#endif
                
} // namespace WebCore

#ifndef NDEBUG

int showTree(const WebCore::HistoryItem* item)
{
    return item->showTree();
}

#endif
