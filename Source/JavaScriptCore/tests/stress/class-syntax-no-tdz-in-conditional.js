//@ skip

class A {
    constructor() { }
}

class B extends A {
    constructor(accessThisBeforeSuper) {
        if (accessThisBeforeSuper)
            this;
        else
            super();
    }
}

noInline(B);

for (var i = 0; i < 100000; ++i)
    new B(false);
