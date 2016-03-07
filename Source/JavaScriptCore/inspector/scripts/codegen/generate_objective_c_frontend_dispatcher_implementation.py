#!/usr/bin/env python
#
# Copyright (c) 2014 Apple Inc. All rights reserved.
# Copyright (c) 2014 University of Washington. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
# THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
# THE POSSIBILITY OF SUCH DAMAGE.


import logging
import string
from string import Template

from generate_objective_c import ObjCGenerator
from generator import Generator, ucfirst
from generator_templates import GeneratorTemplates as Templates

log = logging.getLogger('global')


class ObjectiveCFrontendDispatcherImplementationGenerator(Generator):
    def __init__(self, model, input_filepath):
        Generator.__init__(self, model, input_filepath)

    def output_filename(self):
        return '%sEventDispatchers.mm' % ObjCGenerator.OBJC_PREFIX

    def domains_to_generate(self):
        return filter(ObjCGenerator.should_generate_domain_event_dispatcher_filter(self.model()), Generator.domains_to_generate(self))

    def generate_output(self):
        secondary_headers = [
            '"%sEnumConversionHelpers.h"' % ObjCGenerator.OBJC_PREFIX,
            '<JavaScriptCore/InspectorFrontendChannel.h>',
            '<JavaScriptCore/InspectorValues.h>',
        ]

        header_args = {
            'primaryInclude': '"%sInternal.h"' % ObjCGenerator.OBJC_PREFIX,
            'secondaryIncludes': '\n'.join(['#import %s' % header for header in secondary_headers]),
        }

        domains = self.domains_to_generate()
        sections = []
        sections.append(self.generate_license())
        sections.append(Template(Templates.ObjCImplementationPrelude).substitute(None, **header_args))
        sections.extend(map(self._generate_event_dispatcher_implementations, domains))
        sections.append(Template(Templates.ObjCImplementationPostlude).substitute(None, **header_args))
        return '\n\n'.join(sections)

    def _generate_event_dispatcher_implementations(self, domain):
        if not domain.events:
            return ''

        lines = []
        objc_name = '%s%sDomainEventDispatcher' % (ObjCGenerator.OBJC_PREFIX, domain.domain_name)
        lines.append('@implementation %s' % objc_name)
        lines.append('{')
        lines.append('    AugmentableInspectorController* _controller;')
        lines.append('}')
        lines.append('')
        lines.append('- (instancetype)initWithController:(AugmentableInspectorController*)controller;')
        lines.append('{')
        lines.append('    self = [super init];')
        lines.append('    if (!self)')
        lines.append('        return nil;')
        lines.append('    ASSERT(controller);')
        lines.append('    _controller = controller;')
        lines.append('    return self;')
        lines.append('}')
        lines.append('')
        for event in domain.events:
            lines.append(self._generate_event(domain, event))
            lines.append('')
        lines.append('@end')
        return '\n'.join(lines)

    def _generate_event(self, domain, event):
        lines = []
        lines.append(self._generate_event_signature(domain, event))
        lines.append('{')
        lines.append('    InspectorFrontendChannel* frontendChannel = _controller->frontendChannel();')
        lines.append('    if (!frontendChannel)')
        lines.append('        return;')
        lines.append('')
        lines.append('    RefPtr<InspectorObject> jsonMessage = InspectorObject::create();')
        lines.append('    jsonMessage->setString(ASCIILiteral("method"), ASCIILiteral("%s.%s"));' % (domain.domain_name, event.event_name))
        if event.event_parameters:
            lines.extend(self._generate_event_out_parameters(domain, event))
        lines.append('    frontendChannel->sendMessageToFrontend(jsonMessage->toJSONString());')
        lines.append('}')
        return '\n'.join(lines)

    def _generate_event_signature(self, domain, event):
        if not event.event_parameters:
            return '- (void)%s' % event.event_name
        pairs = []
        for parameter in event.event_parameters:
            param_name = parameter.parameter_name
            pairs.append('%s:(%s)%s' % (param_name, ObjCGenerator.objc_type_for_param(domain, event.event_name, parameter), param_name))
        pairs[0] = ucfirst(pairs[0])
        return '- (void)%sWith%s' % (event.event_name, ' '.join(pairs))

    def _generate_event_out_parameters(self, domain, event):
        lines = []
        lines.append('    RefPtr<InspectorObject> paramsObject = InspectorObject::create();')
        for parameter in event.event_parameters:
            keyed_set_method = Generator.keyed_set_method_for_type(parameter.type)
            var_name = parameter.parameter_name
            safe_var_name = '(*%s)' % var_name if parameter.is_optional else var_name
            export_expression = ObjCGenerator.objc_protocol_export_expression_for_variable(parameter.type, safe_var_name)
            if not parameter.is_optional:
                lines.append('    paramsObject->%s(ASCIILiteral("%s"), %s);' % (keyed_set_method, parameter.parameter_name, export_expression))
            else:
                lines.append('    if (%s)' % (parameter.parameter_name))
                lines.append('        paramsObject->%s(ASCIILiteral("%s"), %s);' % (keyed_set_method, parameter.parameter_name, export_expression))
        lines.append('    jsonMessage->setObject(ASCIILiteral("params"), paramsObject);')
        return lines
