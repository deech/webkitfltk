/*
    This file is part of the WebKit open source project.
    This file has been generated by generate-bindings.pl. DO NOT MODIFY!

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef JSTestNondeterministic_h
#define JSTestNondeterministic_h

#include "JSDOMWrapper.h"
#include "TestNondeterministic.h"

namespace WebCore {

class JSTestNondeterministic : public JSDOMWrapper {
public:
    typedef JSDOMWrapper Base;
    static JSTestNondeterministic* create(JSC::Structure* structure, JSDOMGlobalObject* globalObject, Ref<TestNondeterministic>&& impl)
    {
        JSTestNondeterministic* ptr = new (NotNull, JSC::allocateCell<JSTestNondeterministic>(globalObject->vm().heap)) JSTestNondeterministic(structure, globalObject, WTF::move(impl));
        ptr->finishCreation(globalObject->vm());
        return ptr;
    }

    static JSC::JSObject* createPrototype(JSC::VM&, JSC::JSGlobalObject*);
    static JSC::JSObject* getPrototype(JSC::VM&, JSC::JSGlobalObject*);
    WEBCORE_EXPORT static TestNondeterministic* toWrapped(JSC::JSValue);
    static void destroy(JSC::JSCell*);
    ~JSTestNondeterministic();

    DECLARE_INFO;

    static JSC::Structure* createStructure(JSC::VM& vm, JSC::JSGlobalObject* globalObject, JSC::JSValue prototype)
    {
        return JSC::Structure::create(vm, globalObject, prototype, JSC::TypeInfo(JSC::ObjectType, StructureFlags), info());
    }

    static JSC::JSValue getConstructor(JSC::VM&, JSC::JSGlobalObject*);
    TestNondeterministic& impl() const { return *m_impl; }
    void releaseImpl() { m_impl->deref(); m_impl = 0; }

    void releaseImplIfNotNull()
    {
        if (m_impl) {
            m_impl->deref();
            m_impl = 0;
        }
    }

private:
    TestNondeterministic* m_impl;
protected:
    JSTestNondeterministic(JSC::Structure*, JSDOMGlobalObject*, Ref<TestNondeterministic>&&);

    void finishCreation(JSC::VM& vm)
    {
        Base::finishCreation(vm);
        ASSERT(inherits(info()));
    }

};

class JSTestNondeterministicOwner : public JSC::WeakHandleOwner {
public:
    virtual bool isReachableFromOpaqueRoots(JSC::Handle<JSC::Unknown>, void* context, JSC::SlotVisitor&);
    virtual void finalize(JSC::Handle<JSC::Unknown>, void* context);
};

inline JSC::WeakHandleOwner* wrapperOwner(DOMWrapperWorld&, TestNondeterministic*)
{
    DEPRECATED_DEFINE_STATIC_LOCAL(JSTestNondeterministicOwner, jsTestNondeterministicOwner, ());
    return &jsTestNondeterministicOwner;
}

WEBCORE_EXPORT JSC::JSValue toJS(JSC::ExecState*, JSDOMGlobalObject*, TestNondeterministic*);
inline JSC::JSValue toJS(JSC::ExecState* exec, JSDOMGlobalObject* globalObject, TestNondeterministic& impl) { return toJS(exec, globalObject, &impl); }


} // namespace WebCore

#endif
