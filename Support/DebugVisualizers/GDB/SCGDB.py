# Copyright (c) Stefano Cristiano
# SPDX-License-Identifier: MIT
import gdb
import re

class VectorPrettyPrinter:
    def __init__(self, val):
        self.val = val
        self.header = self.val['header']
        self.size_bytes = int(self.header['sizeBytes'])
        self.capacity_bytes = int(self.header['capacityBytes'])
        self.data_type = self.val.type.template_argument(0)
        self.item_size = self.data_type.sizeof
        self.size = self.size_bytes // self.item_size
        self.capacity = self.capacity_bytes // self.item_size

    def to_string(self):
        return f"size={self.size}, capacity={self.capacity}"

    def children(self):
        # For SC::Array, use direct items field access (no offset needed)
        valname = str(self.val.type)
        if 'Array' in valname:
            try:
                items = self.val['items']
                for i in range(self.size):
                    try:
                        yield f"[{i}]", items[i]
                    except:
                        break
                return
            except Exception as e:
                gdb.write(f"Array items access failed: {e}\n")
                pass

        # For SC::Vector and SC::SmallVector, use offset calculation
        try:
            offset_field = self.val['offset']
            if offset_field and not offset_field.is_optimized_out:
                offset = int(offset_field)
                # Use proper GDB address calculation
                base_addr = self.val.address
                # Cast to char pointer, add offset, then cast to target type
                char_type = gdb.lookup_type('char')
                char_ptr = base_addr.cast(char_type.pointer())
                items_addr = char_ptr + offset
                items = items_addr.cast(self.data_type.pointer())
                for i in range(self.size):
                    try:
                        yield f"[{i}]", items[i]
                    except:
                        break
                return
        except Exception as e:
            gdb.write(f"Vector offset calculation failed: {e}\n")
            pass

    def display_hint(self):
        return 'array'

class BufferPrettyPrinter:
    def __init__(self, val):
        self.val = val
        self.header = self.val['header']
        self.size_bytes = int(self.header['sizeBytes'])
        self.capacity_bytes = int(self.header['capacityBytes'])
        self.size = self.size_bytes
        self.capacity = self.capacity_bytes

    def to_string(self):
        return f"size={self.size}, capacity={self.capacity}"

    def children(self):
        # Use offset calculation method (works reliably)
        try:
            offset_field = self.val['offset']
            if offset_field and not offset_field.is_optimized_out:
                offset = int(offset_field)
                # Use proper GDB address calculation
                base_addr = self.val.address
                char_type = gdb.lookup_type('char')
                char_ptr = base_addr.cast(char_type.pointer())
                items_addr = char_ptr + offset
                items = items_addr.cast(char_type.pointer())
                for i in range(self.size):
                    try:
                        yield f"[{i}]", items[i]
                    except:
                        break
                return
        except Exception as e:
            gdb.write(f"Buffer offset calculation failed: {e}\n")
            pass

    def display_hint(self):
        return 'array'

class StringPrettyPrinter:
    def __init__(self, val):
        self.val = val

    def to_string(self):
        try:
            valname = str(self.val.type)

            # Handle StringView and StringSpan differently
            if valname in ("SC::StringView", "SC::StringSpan"):
                if valname == "SC::StringView":
                    # StringView might be a wrapper around StringSpan
                    try:
                        # Try direct access first (StringView might have the members directly)
                        text_size = int(self.val['textSizeInBytes'])
                        text_ptr = self.val['text']
                        encoding_val = self.val['encoding']
                    except:
                        # If direct access fails, try getting the span member
                        try:
                            span_val = self.val['span']
                            text_size = int(span_val['textSizeInBytes'])
                            text_ptr = span_val['text']
                            encoding_val = span_val['encoding']
                        except:
                            return '""'
                else:
                    # StringSpan has direct members
                    text_size = int(self.val['textSizeInBytes'])
                    text_ptr = self.val['text']
                    encoding_val = self.val['encoding']

                if text_size == 0:
                    return '""'

                # Try to get encoding
                try:
                    encoding_value = int(encoding_val)
                except:
                    encoding_value = 1  # Default to UTF-8

                # Read the string data
                try:
                    if encoding_value == 0 or encoding_value == 1:  # ascii or utf8
                        string_data = text_ptr.string(length=text_size)
                        return f'"{string_data}"'
                    elif encoding_value == 2:  # UTF-16
                        bytes_data = bytes(text_ptr[i] for i in range(text_size))
                        decoded = bytes_data.decode('utf-16-le')
                        return f'"{decoded}"'
                    else:
                        return "<error: corrupted encoding tag>"
                except Exception as e:
                    return f"<error reading string data: {e}>"

            # Handle SC::String and SC::SmallString
            else:
                # For SC::String, try to access the data buffer directly
                data_val = self.val['data']
                if data_val:
                    # Get buffer information
                    header = data_val['header']
                    size_bytes = int(header['sizeBytes'])
                    if size_bytes <= 1:  # Empty string (just null terminator)
                        return '""'

                    # Try to get encoding
                    try:
                        encoding = self.val['encoding']
                        encoding_value = int(encoding)
                        # Adjust for null terminator
                        if encoding_value == 0 or encoding_value == 1:  # ascii or utf8
                            text_size = size_bytes - 1
                        else:
                            text_size = size_bytes - 2
                    except:
                        # Default to UTF-8 if encoding not accessible
                        text_size = size_bytes - 1
                        encoding_value = 1

                    # Calculate data pointer
                    try:
                        offset = int(data_val['offset'])
                        base_addr = data_val.address
                        char_type = gdb.lookup_type('char')
                        char_ptr = base_addr.cast(char_type.pointer())
                        text_addr = char_ptr + offset
                        text = text_addr.cast(char_type.pointer())

                        # Read the string data
                        if encoding_value == 0 or encoding_value == 1:  # ascii or utf8
                            string_data = text.string(length=text_size)
                            return f'"{string_data}"'
                        elif encoding_value == 2:  # UTF-16
                            bytes_data = bytes(text[i] for i in range(text_size))
                            decoded = bytes_data.decode('utf-16-le')
                            return f'"{decoded}"'
                        else:
                            return "<error: corrupted encoding tag>"
                    except Exception as e:
                        return f"<error reading string data: {e}>"
                else:
                    return '""'
        except Exception as e:
            return f"<string error: {e}>"

class SpanPrettyPrinter:
    def __init__(self, val):
        self.val = val
        self.data_type = self.val.type.template_argument(0)
        self.item_size = self.data_type.sizeof
        self.size = int(self.val['sizeElements'])
        self.items = self.val['items']

    def to_string(self):
        return f"size={self.size}"

    def children(self):
        try:
            # Direct access to items for Span (no offset calculation needed)
            items = self.items
            for i in range(self.size):
                try:
                    yield f"[{i}]", items[i]
                except:
                    break
        except Exception as e:
            gdb.write(f"Span items access failed: {e}\n")
            pass

    def display_hint(self):
        return 'array'

def build_pretty_printer():
    pp = gdb.printing.RegexpCollectionPrettyPrinter("SC")

    # Vector types
    pp.add_printer('SC::Vector', '^SC::Vector<.*>$', VectorPrettyPrinter)
    pp.add_printer('SC::SmallVector', '^SC::SmallVector<.*>$', VectorPrettyPrinter)
    pp.add_printer('SC::Array', '^SC::Array<.*>$', VectorPrettyPrinter)

    # Buffer types
    pp.add_printer('SC::Buffer', '^SC::Buffer$', BufferPrettyPrinter)
    pp.add_printer('SC::SmallBuffer', '^SC::SmallBuffer<.*>$', BufferPrettyPrinter)

    # String types
    pp.add_printer('SC::String', '^SC::String$', StringPrettyPrinter)
    pp.add_printer('SC::SmallString', '^SC::SmallString<.*>$', StringPrettyPrinter)
    pp.add_printer('SC::StringSpan', '^SC::StringSpan$', StringPrettyPrinter)
    pp.add_printer('SC::StringView', '^SC::StringView$', StringPrettyPrinter)

    # Span types
    pp.add_printer('SC::Span', '^SC::Span<.*>$', SpanPrettyPrinter)

    return pp

# Register the pretty printers
gdb.printing.register_pretty_printer(gdb.current_objfile(), build_pretty_printer())
