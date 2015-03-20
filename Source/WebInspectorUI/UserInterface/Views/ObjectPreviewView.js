/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

WebInspector.ObjectPreviewView = function(preview, mode)
{
    // FIXME: Convert this to a WebInspector.Object subclass, and call super().
    // WebInspector.Object.call(this);

    console.assert(preview instanceof WebInspector.ObjectPreview);

    this._preview = preview;
    this._mode = mode || WebInspector.ObjectPreviewView.Mode.Full;

    this._element = document.createElement("span");
    this._element.className = "object-preview";

    this._previewElement = this._element.appendChild(document.createElement("span"));
    this._previewElement.className = "preview";
    this._lossless = this._appendPreview(this._previewElement, this._preview);

    this._titleElement = this._element.appendChild(document.createElement("span"));
    this._titleElement.className = "title";
    this._titleElement.hidden = true;
    this._initTitleElement();

    if (this._preview.hasSize()) {
        var sizeElement = this._element.appendChild(document.createElement("span"));
        sizeElement.className = "size";
        sizeElement.textContent = " (" + this._preview.size + ")";
    }

    if (this._lossless)
        this._element.classList.add("lossless");
};

WebInspector.ObjectPreviewView.Mode = {
    Brief: Symbol("object-preview-brief"),
    Full: Symbol("object-preview-full"),
};

WebInspector.ObjectPreviewView.prototype = {
    constructor: WebInspector.ObjectPreviewView,
    __proto__: WebInspector.Object.prototype,

    // Public

    get preview()
    {
        return this._preview;
    },

    get element()
    {
        return this._element;
    },

    get mode()
    {
        return this._mode;
    },

    get lossless()
    {
        return this._lossless;
    },

    showTitle()
    {
        this._titleElement.hidden = false;
        this._previewElement.hidden = true;
    },

    showPreview()
    {
        this._titleElement.hidden = true;
        this._previewElement.hidden = false;
    },

    // Private

    _initTitleElement()
    {
        // Display null / regexps as simple formatted values even in title.
        if (this._preview.subtype === "regexp" || this._preview.subtype === "null")
            this._titleElement.appendChild(WebInspector.FormattedValue.createElementForObjectPreview(this._preview));
        else
            this._titleElement.textContent = this._preview.description || "";
    },

    _numberOfPropertiesToShowInMode()
    {
        return this._mode === WebInspector.ObjectPreviewView.Mode.Brief ? 3 : Infinity;
    },

    _appendPreview(element, preview)
    {
        var displayObjectAsValue = false;
        if (preview.type === "object") {
            if (preview.subtype === "regexp" || preview.subtype === "null") {
                // Display null / regexps as simple formatted values.
                displayObjectAsValue = true;
            }  else if (preview.subtype !== "array" && preview.description !== "Object") {
                // Class names for other non-array / non-basic-Object types.
                var nameElement = element.appendChild(document.createElement("span"));
                nameElement.className = "object-preview-name";
                nameElement.textContent = preview.description + " ";
            }
        }

        // Content.
        var bodyElement = element.appendChild(document.createElement("span"));
        bodyElement.className = "object-preview-body";
        if (!displayObjectAsValue) {
            if (preview.collectionEntryPreviews)
                return this._appendEntryPreviews(bodyElement, preview);
            if (preview.propertyPreviews)
                return this._appendPropertyPreviews(bodyElement, preview);
        }
        return this._appendValuePreview(bodyElement, preview);
    },

    _appendEntryPreviews(element, preview)
    {
        var lossless = preview.lossless && !preview.propertyPreviews.length;

        var isIterator = preview.subtype === "iterator";

        element.appendChild(document.createTextNode(isIterator ? "[" : "{"));

        var limit = Math.min(preview.collectionEntryPreviews.length, this._numberOfPropertiesToShowInMode());
        for (var i = 0; i < limit; ++i) {
            if (i > 0)
                element.appendChild(document.createTextNode(", "));

            var keyPreviewLossless = true;
            var entry = preview.collectionEntryPreviews[i];
            if (entry.keyPreview) {
                keyPreviewLossless = this._appendPreview(element, entry.keyPreview);
                element.appendChild(document.createTextNode(" => "));
            }

            var valuePreviewLossless = this._appendPreview(element, entry.valuePreview);

            if (!keyPreviewLossless || !valuePreviewLossless)
                lossless = false;
        }

        if (preview.overflow)
            element.appendChild(document.createTextNode(", \u2026"));
        element.appendChild(document.createTextNode(isIterator ? "]" : "}"));

        return lossless;
    },

    _appendPropertyPreviews(element, preview)
    {
        // Do not show Error properties in previews. They are more useful in full views.
        if (preview.subtype === "error")
            return false;

        // Do not show Date properties in previews. If there are any properties, show them in full view.
        if (preview.subtype === "date")
            return !preview.propertyPreviews.length;

        // FIXME: Array previews should have better sparse support: (undefined × 10).
        var isArray = preview.subtype === "array";

        element.appendChild(document.createTextNode(isArray ? "[" : "{"));

        var numberAdded = 0;
        var limit = this._numberOfPropertiesToShowInMode();
        for (var i = 0; i < preview.propertyPreviews.length && numberAdded < limit; ++i) {
            var property = preview.propertyPreviews[i];

            // FIXME: Better handle getter/setter accessors. Should we show getters in previews?
            if (property.type === "accessor")
                continue;

            // Constructor name is often already visible, so don't show it as a property.
            if (property.name === "constructor")
                continue;

            if (numberAdded++ > 0)
                element.appendChild(document.createTextNode(", "));

            if (!isArray || property.name != i) {
                var nameElement = element.appendChild(document.createElement("span"));
                nameElement.className = "name";
                nameElement.textContent = property.name;
                element.appendChild(document.createTextNode(": "));
            }

            if (property.valuePreview)
                this._appendPreview(element, property.valuePreview);
            else
                element.appendChild(WebInspector.FormattedValue.createElementForPropertyPreview(property));
        }

        if (preview.overflow)
            element.appendChild(document.createTextNode(", \u2026"));

        element.appendChild(document.createTextNode(isArray ? "]" : "}"));

        return preview.lossless;
    },

    _appendValuePreview(element, preview)
    {
        element.appendChild(WebInspector.FormattedValue.createElementForObjectPreview(preview));
        return true;
    }
};
