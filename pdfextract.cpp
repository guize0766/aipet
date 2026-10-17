#include "pdfextract.h"

#include <QFile>
#include <QByteArray>
#include <QMap>
#include <QSet>
#include <QList>
#include <QRegularExpression>
#include <QStringDecoder>
#include <zlib.h>

// ---------------------------------------------------------------------------
// 轻量 PDF 文本提取器
// 目标：文本型 PDF（大学课件、Word/PPT 导出的 PDF）
// 能力：对象扫描 → FlateDecode 解压 → ToUnicode CMap → BT/ET 文本操作符提取
// 限制：扫描件/图片版 PDF 无文本层 → 返回空字符串；非常规编码回退为 '?'
// ---------------------------------------------------------------------------

// glyphId(2字节大端) -> Unicode 码点
typedef QMap<int, ushort> GlyphMap;

namespace {

struct PdfObject
{
    QByteArray dictBytes;   // stream 关键字之前的部分（对象字典）
    QByteArray streamRaw;   // stream ... endstream 之间的原始字节
    bool isStream = false;
};

// 扫描 "N 0 obj ... endobj"，把对象体按 latin1 1:1 保留（不破坏二进制流）
QMap<int, PdfObject> parseObjects(const QByteArray &data)
{
    QMap<int, PdfObject> objs;
    QRegularExpression reObj("(\\d+)\\s+0\\s+obj(.*?)endobj",
                             QRegularExpression::DotMatchesEverythingOption);
    const QString src = QString::fromLatin1(data);
    auto it = reObj.globalMatch(src);
    while (it.hasNext()) {
        auto m = it.next();
        int id = m.captured(1).toInt();
        QByteArray body = m.captured(2).toLatin1(); // fromLatin1 之后 toLatin1 是 1:1 还原
        PdfObject obj;
        int si = body.indexOf("stream");
        if (si >= 0) {
            obj.isStream = true;
            obj.dictBytes = body.left(si);
            int start = si + 6;
            while (start < body.size() && (body[start] == '\r' || body[start] == '\n'))
                ++start;
            int ei = body.indexOf("endstream", start);
            if (ei < 0) ei = body.size();
            obj.streamRaw = body.mid(start, ei - start);
            while (obj.streamRaw.endsWith('\n') || obj.streamRaw.endsWith('\r'))
                obj.streamRaw.chop(1);
        } else {
            obj.dictBytes = body;
        }
        objs[id] = obj;
    }
    return objs;
}

// 标准 zlib inflate（PDF FlateDecode 是 RFC1950 zlib 流；qUncompress 不兼容，需直接用 zlib）
QByteArray inflateZlib(const QByteArray &data)
{
    if (data.isEmpty())
        return QByteArray();
    z_stream strm;
    memset(&strm, 0, sizeof(strm));
    if (inflateInit(&strm) != Z_OK)
        return QByteArray();

    QByteArray out;
    const int chunk = 32768;
    QByteArray buf(chunk, Qt::Uninitialized);

    strm.next_in = reinterpret_cast<Bytef *>(const_cast<char *>(data.constData()));
    strm.avail_in = static_cast<uInt>(data.size());

    int ret;
    do {
        strm.next_out = reinterpret_cast<Bytef *>(buf.data());
        strm.avail_out = chunk;
        ret = inflate(&strm, Z_NO_FLUSH);
        if (ret != Z_OK && ret != Z_STREAM_END && ret != Z_BUF_ERROR) {
            inflateEnd(&strm);
            return QByteArray();
        }
        out.append(buf.constData(), chunk - strm.avail_out);
    } while (ret != Z_STREAM_END && strm.avail_in > 0);

    inflateEnd(&strm);
    return (ret == Z_STREAM_END) ? out : QByteArray();
}

// FlateDecode（zlib 格式）解压；未压缩流原样返回
QByteArray decompressStream(const PdfObject &obj)
{
    if (!obj.isStream)
        return QByteArray();
    if (obj.dictBytes.contains("/FlateDecode")) {
        return inflateZlib(obj.streamRaw);
    }
    return obj.streamRaw;
}

// 解析 ToUnicode CMap：bfchar + bfrange（三码形式）
GlyphMap parseCMap(const QByteArray &data)
{
    GlyphMap map;
    const QString s = QString::fromLatin1(data);
    const auto opt = QRegularExpression::DotMatchesEverythingOption
                     | QRegularExpression::CaseInsensitiveOption;

    QRegularExpression reChar("beginbfchar(.*?)endbfchar", opt);
    auto it = reChar.globalMatch(s);
    while (it.hasNext()) {
        const QString body = it.next().captured(1);
        QRegularExpression rePair("<([0-9A-Fa-f]{2,4})>\\s*<([0-9A-Fa-f]{2,4})>");
        auto pit = rePair.globalMatch(body);
        while (pit.hasNext()) {
            auto pm = pit.next();
            bool ok = false;
            const int src = pm.captured(1).toInt(&ok, 16);
            const int dst = pm.captured(2).toInt(&ok, 16);
            if (ok)
                map[src] = ushort(dst);
        }
    }

    QRegularExpression reRange("beginbfrange(.*?)endbfrange", opt);
    auto rit = reRange.globalMatch(s);
    while (rit.hasNext()) {
        const QString body = rit.next().captured(1);
        QRegularExpression reTri("<([0-9A-Fa-f]{4})>\\s*<([0-9A-Fa-f]{4})>\\s*<([0-9A-Fa-f]{4})>");
        auto tit = reTri.globalMatch(body);
        while (tit.hasNext()) {
            auto tm = tit.next();
            bool ok = false;
            const int lo = tm.captured(1).toInt(&ok, 16);
            const int hi = tm.captured(2).toInt(&ok, 16);
            const int base = tm.captured(3).toInt(&ok, 16);
            if (ok) {
                for (int id = lo; id <= hi && id <= 0xFFFF; ++id)
                    map[id] = ushort(base + (id - lo));
            }
        }
    }
    return map;
}

// 汇总文档中所有字体对象的 ToUnicode 映射（全局表，覆盖常见课件 PDF）
GlyphMap buildGlyphMap(const QMap<int, PdfObject> &objs)
{
    GlyphMap map;
    QRegularExpression reTU("/ToUnicode\\s+(\\d+)\\s+0\\s+R");
    for (auto it = objs.cbegin(); it != objs.cend(); ++it) {
        auto m = reTU.match(QString::fromLatin1(it.value().dictBytes));
        if (!m.hasMatch())
            continue;
        const int ref = m.captured(1).toInt();
        auto f = objs.constFind(ref);
        if (f == objs.cend())
            continue;
        const QByteArray cmap = decompressStream(f.value());
        if (!cmap.isEmpty()) {
            const GlyphMap m2 = parseCMap(cmap);
            for (auto x = m2.cbegin(); x != m2.cend(); ++x)
                map[x.key()] = x.value();
        }
    }
    return map;
}

// 读取字面量字符串 ( ... )，处理转义，返回原始字节
QByteArray readLiteralString(const QString &block, int &i)
{
    QByteArray bytes;
    int depth = 1;
    ++i;
    const int n = block.size();
    while (i < n && depth > 0) {
        const QChar ch = block.at(i);
        if (ch == '\\') {
            if (i + 1 >= n) break;
            const QChar esc = block.at(i + 1);
            i += 2;
            switch (esc.unicode()) {
            case 'n': bytes.append('\n'); break;
            case 'r': bytes.append('\r'); break;
            case 't': bytes.append('\t'); break;
            case 'b': bytes.append('\b'); break;
            case 'f': bytes.append('\f'); break;
            case '(': bytes.append('('); break;
            case ')': bytes.append(')'); break;
            case '\\': bytes.append('\\'); break;
            default:
                if (esc.unicode() >= '0' && esc.unicode() <= '7') {
                    int val = esc.unicode() - '0';
                    int cnt = 1;
                    while (i < n && cnt < 3
                           && block.at(i).unicode() >= '0' && block.at(i).unicode() <= '7') {
                        val = val * 8 + (block.at(i).unicode() - '0');
                        ++i; ++cnt;
                    }
                    bytes.append(char(val));
                } else {
                    bytes.append(esc.toLatin1());
                }
            }
        } else if (ch == '(') {
            ++depth; ++i;
        } else if (ch == ')') {
            --depth; ++i;
        } else {
            bytes.append(char(ch.unicode() & 0xFF));
            ++i;
        }
    }
    return bytes;
}

// 读取十六进制字符串 <...>
QByteArray readHexString(const QString &block, int &i)
{
    QByteArray hex;
    ++i;
    const int n = block.size();
    while (i < n && block.at(i) != '>') {
        const QChar ch = block.at(i);
        if (!ch.isSpace())
            hex.append(ch.toLatin1());
        ++i;
    }
    if (i < n) ++i; // 跳过 '>'
    if (hex.size() % 2) hex.append('0');
    return QByteArray::fromHex(hex);
}

// 字面量字节 → 文本：优先按 UTF-8（中文课件常用），否则回退 Windows-1252
QString decodeLiteralBytes(const QByteArray &bytes)
{
    const QString utf8 = QString::fromUtf8(bytes);
    if (!utf8.contains(QChar(0xFFFD)))
        return utf8;
    static QStringDecoder cp1252("Windows-1252");
    return cp1252.decode(bytes);
}

// hex 字形序列 → 文本（2 字节一个大端 glyph id，查 ToUnicode 表）
QString decodeGlyphs(const QByteArray &hex, const GlyphMap &map)
{
    QString out;
    for (int i = 0; i + 1 < hex.size(); i += 2) {
        const int id = (unsigned char)hex.at(i) * 256 + (unsigned char)hex.at(i + 1);
        const int u = map.value(id, -1);
        out += (u >= 0) ? QChar(ushort(u)) : QLatin1Char('?');
    }
    return out;
}

// 解析内容流中的 BT...ET 文本块
QString decodeContentStream(const QByteArray &stream, const GlyphMap &map)
{
    QString text;
    const QString s = QString::fromLatin1(stream);
    QRegularExpression reBlock("BT(.*?)ET",
        QRegularExpression::DotMatchesEverythingOption
        | QRegularExpression::CaseInsensitiveOption);
    auto it = reBlock.globalMatch(s);
    while (it.hasNext()) {
        const QString block = it.next().captured(1);
        int i = 0;
        const int n = block.size();
        while (i < n) {
            const QChar c = block.at(i);
            if (c == '(') {
                text += decodeLiteralBytes(readLiteralString(block, i));
            } else if (c == '<') {
                text += decodeGlyphs(readHexString(block, i), map);
            } else {
                ++i;
            }
        }
        text += QLatin1Char('\n');
    }
    return text;
}

// 收集所有页面 Contents 流引用（按 Kids 顺序）
QList<int> collectPageContents(const QMap<int, PdfObject> &objs, const QByteArray &data)
{
    QList<int> contents;
    QRegularExpression reRef("(\\d+)\\s+0\\s+R");

    QRegularExpression reRoot("/Root\\s+(\\d+)\\s+0\\s+R");
    auto rm = reRoot.match(QString::fromLatin1(data));
    const int rootId = rm.hasMatch() ? rm.captured(1).toInt() : 1;

    QList<int> pending;
    QRegularExpression rePages("/Pages\\s+(\\d+)\\s+0\\s+R");
    auto pm = rePages.match(QString::fromLatin1(objs.value(rootId).dictBytes));
    if (pm.hasMatch())
        pending << pm.captured(1).toInt();

    QSet<int> visited;
    while (!pending.isEmpty()) {
        const int id = pending.takeFirst();
        if (visited.contains(id))
            continue;
        visited.insert(id);
        auto it = objs.constFind(id);
        if (it == objs.cend())
            continue;
        const QString dict = QString::fromLatin1(it.value().dictBytes);

        // /Type /Page 后面不能跟 s（避免把 /Type /Pages 节点误判成页面）
        QRegularExpression reIsPage("/Type\\s*/Page(?!s)");
        if (reIsPage.match(dict).hasMatch()) {
            QRegularExpression reContents("/Contents\\s*(\\[[^\\]]*\\]|\\d+\\s+0\\s+R)");
            auto cm = reContents.match(dict);
            if (cm.hasMatch()) {
                auto rit = reRef.globalMatch(cm.captured(1));
                while (rit.hasNext())
                    contents << rit.next().captured(1).toInt();
            }
        } else {
            QRegularExpression reKids("/Kids\\s*\\[([^\\]]*)\\]");
            auto km = reKids.match(dict);
            if (km.hasMatch()) {
                auto rit = reRef.globalMatch(km.captured(1));
                while (rit.hasNext())
                    pending << rit.next().captured(1).toInt();
            }
        }
    }
    return contents;
}

} // namespace

QString extractPdfTextSimple(const QString &filePath)
{
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly))
        return QString();
    const QByteArray data = f.readAll();
    f.close();

    if (!data.startsWith("%PDF"))
        return QString();

    const QMap<int, PdfObject> objs = parseObjects(data);
    if (objs.isEmpty())
        return QString();

    const GlyphMap glyphMap = buildGlyphMap(objs);
    QList<int> contents = collectPageContents(objs, data);
    if (contents.size() > 30)
        contents = contents.mid(0, 30); // 限制页数，防超大文档

    QString fullText;
    for (int id : contents) {
        auto it = objs.constFind(id);
        if (it == objs.cend())
            continue;
        const QByteArray stream = decompressStream(it.value());
        if (stream.isEmpty())
            continue;
        const QString pageText = decodeContentStream(stream, glyphMap);
        if (!pageText.trimmed().isEmpty())
            fullText += pageText;
    }
    return fullText;
}
