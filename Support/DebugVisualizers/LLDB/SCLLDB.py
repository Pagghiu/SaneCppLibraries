# Copyright (c) Stefano Cristiano
# SPDX-License-Identifier: MIT
import lldb

class vector_SynthProvider:
    def __init__(self, valobj, dict, name="vector_SynthProvider"):
        self.__name = name
        # logger = lldb.formatters.Logger.Logger()
        # logger >> f"{self.__name}.__init__ "
        self.valobj = valobj
        self.items = None
        self.data_size = 0
        self.data_capacity = 0
        self.header_size = self.find_segment_header_size()

    def num_children(self):
        # logger = lldb.formatters.Logger.Logger()
        # logger >> f"{self.__name}.num_children"
        try:
            if not self.items:
                self.update()
            return self.data_size
        except Exception as e:
            # logger >> f"{self.__name} EXCEPTION\n{e}"
            return 0

    def get_child_index(self, name):
        # logger = lldb.formatters.Logger.Logger()
        # logger >> f"{self.__name}.get_child_index"
        try:
            return int(name.lstrip("[").rstrip("]"))
        except Exception as e:
            # logger >> f"{self.__name} EXCEPTION\n{e}"
            return -1

    def get_child_at_index(self, index):
        # logger = lldb.formatters.Logger.Logger()
        # logger >> f"{self.__name}.Retrieving child " + str(index)
        if index < 0:
            return None
        if index >= self.num_children():
            return None
        try:
            if self.isArray:
                return self.items.GetChildAtIndex(index)
            else:
                offset = index * self.item_size
                return self.items.CreateChildAtOffset("[" + str(index) + "]", offset, self.data_type)
        except Exception as e:
            # logger >> f"{self.__name} EXCEPTION\n{e}"
            return None

    def update(self):
        # logger = lldb.formatters.Logger.Logger()
        # logger >> f"{self.__name}.update"
        try:
            self.header = self.valobj.GetChildMemberWithName("header")
            self.data_size_bytes = self.header.GetChildMemberWithName("sizeBytes").GetValueAsUnsigned()
            self.data_capacity_bytes = self.header.GetChildMemberWithName("capacityBytes").GetValueAsUnsigned()

            self.data_type = self.valobj.GetType().GetTemplateArgumentType(0)
            self.item_size = self.data_type.GetByteSize()
            self.data_size = self.data_size_bytes // self.item_size
            self.data_capacity = self.data_capacity_bytes // self.item_size

            offset_field = self.valobj.GetChildMemberWithName("offset")
            self.isArray = not offset_field.IsValid()
            if self.isArray:
                self.items = self.valobj.GetChildAtIndex(1).GetChildAtIndex(0) # Items inside union
            else:
                pointer_to_this = self.valobj.AddressOf().GetValueAsSigned()
                offset = offset_field.GetValueAsSigned()
                self.items = self.valobj.CreateValueFromAddress("items",  pointer_to_this + offset, self.data_type)

            # logger >> f"{self.__name}.size_bytes = {self.data_size_bytes} capacity_bytes = {self.data_capacity_bytes}"
            # logger >> f"{self.__name}.size = {self.data_size} capacity = {self.data_capacity}"
            # logger >> f"{self.__name}.data_type = {self.data_type}"
        except Exception as e:
            # logger >> f"{self.__name} EXCEPTION\n{e}"
            pass

    def has_children(self):
        # logger = lldb.formatters.Logger.Logger()
        # logger >> f"{self.__name}.has_children"
        return True
    
    def find_segment_header_size(self):
        # logger = lldb.formatters.Logger.Logger()
        target = lldb.debugger.GetSelectedTarget()
        # Find the type by name
        type = target.FindFirstType("SC::SegmentHeader")
        return type.GetByteSize()

def vector_SummaryProvider(valobj, dict):
    # logger = lldb.formatters.Logger.Logger()
    raw = valobj.GetNonSyntheticValue()
    prov = vector_SynthProvider(raw, None, "vector_SummaryProvider")
    return f"size={prov.num_children()} capacity={prov.data_capacity}"

class buffer_SynthProvider:
    def __init__(self, valobj, dict, name="buffer_SynthProvider"):
        self.__name = name
        # logger = lldb.formatters.Logger.Logger()
        # logger >> f"{self.__name}.__init__ "
        self.valobj = valobj
        self.items = None
        self.data_size = 0
        self.data_capacity = 0
        self.header_size = self.find_buffer_header_size()

    def num_children(self):
        # logger = lldb.formatters.Logger.Logger()
        # logger >> f"{self.__name}.num_children"
        try:
            if not self.items:
                self.update()
            # logger >> f"returning data_size={self.data_size}"
            return self.data_size
        except:
            # logger >> f"{self.__name} EXCEPTION\n{e}"
            return 0

    def get_child_index(self, name):
        # logger = lldb.formatters.Logger.Logger()
        # logger >> f"{self.__name}.get_child_index"
        try:
            return int(name.lstrip("[").rstrip("]"))
        except Exception as e:
            # logger >> f"{self.__name} EXCEPTION\n{e}"
            return -1

    def get_child_at_index(self, index):
        # logger = lldb.formatters.Logger.Logger()
        # logger >> f"{self.__name}.Retrieving child " + str(index)
        if index < 0:
            return None
        if index >= self.num_children():
            return None
        try:
            offset = index
            return self.items.CreateChildAtOffset("[" + str(index) + "]", offset, self.data_type)
        except Exception as e:
            # logger >> f"{self.__name} EXCEPTION\n{e}"
            return None

    def update(self):
        # logger = lldb.formatters.Logger.Logger()
        # logger >> f"{self.__name}.update"
        try:
            self.header = self.valobj.GetChildMemberWithName("header")
            self.data_size_bytes = self.header.GetChildMemberWithName("sizeBytes").GetValueAsUnsigned()
            self.data_capacity_bytes = self.header.GetChildMemberWithName("capacityBytes").GetValueAsUnsigned()

            target = lldb.debugger.GetSelectedTarget()
            self.data_type = target.FindFirstType("char")
            offset = self.valobj.GetChildMemberWithName("offset").GetValueAsSigned()
            pointer_to_this = self.valobj.AddressOf().GetValueAsSigned()
            self.items = self.valobj.CreateValueFromAddress("items",  pointer_to_this + offset, self.data_type)
            self.data_size = self.data_size_bytes
            self.data_capacity = self.data_capacity_bytes
            # logger >> f"{self.__name}.size_bytes = {self.data_size_bytes} capacity_bytes = {self.data_capacity_bytes}"
            # logger >> f"{self.__name}.size = {self.data_size} capacity = {self.data_capacity}"
            # logger >> f"{self.__name}.data_type = {self.data_type}"
        except Exception as e:
            # logger >> f"{self.__name} EXCEPTION\n{e}"
            pass

    def has_children(self):
        # logger = lldb.formatters.Logger.Logger()
        # logger >> f"{self.__name}.has_children"
        return True
    
    def find_buffer_header_size(self):
        target = lldb.debugger.GetSelectedTarget()
        # Find the type by name
        type = target.FindFirstType("SC::SegmentHeader")
        return type.GetByteSize()


def buffer_SummaryProvider(valobj, dict):
    # logger = lldb.formatters.Logger.Logger()
    raw = valobj.GetNonSyntheticValue()
    prov = buffer_SynthProvider(raw, None, "buffer_SummaryProvider")
    return f"size={prov.num_children()} capacity={prov.data_capacity}"


def string_SummaryProvider(valobj, dict):
    # logger = lldb.formatters.Logger.Logger()
    try:
        encoding = valobj.GetChildMemberWithName("encoding")
        encoding_value = encoding.GetValueAsUnsigned()
        valname = valobj.GetType().GetName()
        if valname in ("SC::StringSpan", "SC::StringView"):
            if valname == "SC::StringView":
                valobj = valobj.GetChildAtIndex(0)
            data_size = valobj.GetChildMemberWithName("textSizeInBytes").GetValueAsUnsigned()
            if data_size == 0:
                return '""'
            data = valobj.GetChildMemberWithName("text").GetPointeeData(0, data_size)
        else:
            data = valobj.GetChildMemberWithName("data").GetNonSyntheticValue()
            prov = buffer_SynthProvider(data, None, "string_SummaryProvider")
            prov.update()
            items = prov.items
            if prov.data_size == 0:
                return '""'
            # Remove null terminator
            data_size = prov.data_size
            if encoding_value == 0 or encoding_value == 1:
                data_size = data_size - 1
            else:
                data_size = data_size - 2
            data = items.AddressOf().GetPointeeData(0, data_size)
        error = lldb.SBError()

        if encoding_value == 0 or encoding_value == 1: # ascii or utf8
            # strval = data.GetString(error, 0)
            # if error.Fail():
            #     return "<error:" + error.GetCString() + ">"
            # else:
            #     return '"' + strval + '"'
            byte_buffer = data.ReadRawData(error, int(0), data_size)
            if not error.Fail():
                try:
                    # Decode the byte_buffer directly
                    decoded_str : str =  byte_buffer.decode('utf-8')
                    return '"' + decoded_str + '"'
                except UnicodeDecodeError:
                    return "<error: unable to decode as UTF-8>"
            else:
                return "<error: unable to read memory for UTF-8>"

        elif encoding_value == 2:  # UTF-16
            byte_buffer = data.ReadRawData(error, int(0), data_size)
            if not error.Fail():
                try:
                    # Decode the byte_buffer directly
                    decoded_str = byte_buffer.decode('utf-16-le')
                    return f'"{decoded_str}"'
                except UnicodeDecodeError:
                    return "<error: unable to decode as UTF-16>"
            else:
                return "<error: unable to read memory for UTF-16>"
        else:
            return "<error: corrupted encoding tag>"
    except Exception as e:
        # logger >> f"string_SummaryProvider EXCEPTION {str(e)}"
        pass
    return '""'

class span_SynthProvider:
    def __init__(self, valobj, dict, name="span_SynthProvider"):
        self.__name = name
        # logger = lldb.formatters.Logger.Logger()
        # logger >> f"{self.__name}.__init__ "
        self.valobj = valobj
        self.items = None
        self.data_size = 0
        self.data_type = None
        self.item_size = 0

    def num_children(self):
        # logger = lldb.formatters.Logger.Logger()
        # logger >> f"{self.__name}.num_children"
        try:
            if not self.items:
                self.update()
            return self.data_size
        except Exception as e:
            # logger >> f"{self.__name} EXCEPTION\n{e}"
            return 0

    def get_child_index(self, name):
        # logger = lldb.formatters.Logger.Logger()
        # logger >> f"{self.__name}.get_child_index"
        try:
            return int(name.lstrip("[").rstrip("]"))
        except Exception as e:
            # logger >> f"{self.__name} EXCEPTION\n{e}"
            return -1

    def get_child_at_index(self, index):
        # logger = lldb.formatters.Logger.Logger()
        # logger >> f"{self.__name}.Retrieving child " + str(index)
        if index < 0:
            return None
        if index >= self.num_children():
            return None
        try:
            offset = index * self.item_size
            return self.items.CreateChildAtOffset("[" + str(index) + "]", offset, self.data_type)
        except Exception as e:
            # logger >> f"{self.__name} EXCEPTION\n{e}"
            return None

    def update(self):
        # logger = lldb.formatters.Logger.Logger()
        # logger >> f"{self.__name}.update"
        try:
            # Get the template argument type (the Type parameter)
            self.data_type = self.valobj.GetType().GetTemplateArgumentType(0)
            self.item_size = self.data_type.GetByteSize()

            # Get the items pointer and size
            items_ptr = self.valobj.GetChildMemberWithName("items")
            size_elements = self.valobj.GetChildMemberWithName("sizeElements").GetValueAsUnsigned()

            self.data_size = size_elements
            self.items = items_ptr.Dereference()

            # logger >> f"{self.__name}.size = {self.data_size}"
            # logger >> f"{self.__name}.data_type = {self.data_type}"
        except Exception as e:
            # logger >> f"{self.__name} EXCEPTION\n{e}"
            pass

    def has_children(self):
        # logger = lldb.formatters.Logger.Logger()
        # logger >> f"{self.__name}.has_children"
        return True

def span_SummaryProvider(valobj, dict):
    # logger = lldb.formatters.Logger.Logger()
    raw = valobj.GetNonSyntheticValue()
    prov = span_SynthProvider(raw, None, "span_SummaryProvider")
    return f"size={prov.num_children()}"


# Add your CustomVector class type here
def __lldb_init_module(debugger, internal_dict):
    lldb.formatters.Logger._lldb_formatters_debug_level = 2

    ###########################################################################################################
    # Buffers
    ###########################################################################################################
    # SmallBuffer must be registered BEFORE Buffer in order for the synthetic provider to be matched first
    debugger.HandleCommand('type synthetic add -l SCLLDB.buffer_SynthProvider -x "^(SC::)SmallBuffer<.+>$"')
    debugger.HandleCommand('type summary add -F SCLLDB.buffer_SummaryProvider -e -x "^(SC::)SmallBuffer<.+>$"')

    debugger.HandleCommand('type synthetic add -l SCLLDB.buffer_SynthProvider -x "^(SC::)Buffer$"')
    debugger.HandleCommand('type summary add -F SCLLDB.buffer_SummaryProvider -e -x "^(SC::)Buffer$"')

    ###########################################################################################################
    # Arrays
    ###########################################################################################################
    debugger.HandleCommand('type synthetic add -l SCLLDB.vector_SynthProvider -x "^(SC::)Array<.+>$"')
    debugger.HandleCommand('type summary add -F SCLLDB.vector_SummaryProvider -e -x "^(SC::)Array<.+>$"')

    ###########################################################################################################
    # Vectors
    ###########################################################################################################
    # SmallVector must be registered BEFORE Vector in order for the synthetic provider to be matched first
    debugger.HandleCommand('type synthetic add -l SCLLDB.vector_SynthProvider -x "^(SC::)SmallVector<.+,.+>$"')
    debugger.HandleCommand('type summary add -F SCLLDB.vector_SummaryProvider -e -x "^(SC::)SmallVector<.+,.+>$"')

    debugger.HandleCommand('type synthetic add -l SCLLDB.vector_SynthProvider -x "^(SC::)Vector<.+>$"')
    debugger.HandleCommand('type summary add -F SCLLDB.vector_SummaryProvider -e -x "^(SC::)Vector<.+>$"')

    ###########################################################################################################
    # Strings
    ###########################################################################################################
    debugger.HandleCommand('type summary add -F SCLLDB.string_SummaryProvider -e -x "^(SC::)SmallString<.+>$"')
    debugger.HandleCommand('type summary add -F SCLLDB.string_SummaryProvider -e -x "^(SC::)String$"')
    debugger.HandleCommand('type summary add -F SCLLDB.string_SummaryProvider -e -x "^(SC::)StringView$"')
    debugger.HandleCommand('type summary add -F SCLLDB.string_SummaryProvider -e -x "^(SC::)StringSpan$"')

    ###########################################################################################################
    # Spans
    ###########################################################################################################
    debugger.HandleCommand('type synthetic add -l SCLLDB.span_SynthProvider -x "^(SC::)Span<.+>$"')
    debugger.HandleCommand('type summary add -F SCLLDB.span_SummaryProvider -e -x "^(SC::)Span<.+>$"')
