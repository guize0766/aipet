#ifndef PDFEXTRACT_H
#define PDFEXTRACT_H

#include <QString>

// 轻量 PDF 文本提取：支持文本型 PDF（FlateDecode 压缩流 + ToUnicode 映射 + 常见 ASCII/WinAnsi 文本）。
// 扫描件/图片版 PDF 无文本层，返回空字符串由调用方提示。
// 无第三方依赖，专为大学课件这类文本型 PDF 设计。
QString extractPdfTextSimple(const QString &filePath);

#endif // PDFEXTRACT_H
