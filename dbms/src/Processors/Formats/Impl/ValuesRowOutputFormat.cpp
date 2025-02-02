#include <Processors/Formats/Impl/ValuesRowOutputFormat.h>
#include <Formats/FormatFactory.h>

#include <IO/WriteHelpers.h>
#include <Columns/IColumn.h>
#include <DataTypes/IDataType.h>


namespace DB
{


ValuesRowOutputFormat::ValuesRowOutputFormat(WriteBuffer & out_, const Block & header_, const FormatSettings & format_settings_)
    : IRowOutputFormat(header_, out_), format_settings(format_settings_)
{
}

void ValuesRowOutputFormat::writeField(const IColumn & column, const IDataType & type, size_t row_num)
{
    type.serializeAsTextQuoted(column, row_num, out, format_settings);
}

void ValuesRowOutputFormat::writeFieldDelimiter()
{
    writeChar(',', out);
}

void ValuesRowOutputFormat::writeRowStartDelimiter()
{
    writeChar('(', out);
}

void ValuesRowOutputFormat::writeRowEndDelimiter()
{
    writeChar(')', out);
}

void ValuesRowOutputFormat::writeRowBetweenDelimiter()
{
    writeCString(",", out);
}


void registerOutputFormatProcessorValues(FormatFactory & factory)
{
    factory.registerOutputFormatProcessor("Values", [](
        WriteBuffer & buf,
        const Block & sample,
        const Context &,
        const FormatSettings & settings)
    {
        return std::make_shared<ValuesRowOutputFormat>(buf, sample, settings);
    });
}

}
