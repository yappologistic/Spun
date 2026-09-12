#pragma once
#include <QRegularExpression>
#include <QVariantList>
#include <algorithm>
namespace Lrc {
inline QVariantList parse(QString text, qint64 duration = 0) {
  if (text.size() > 262144)
    return {};
  static const QRegularExpression stamp(
      R"(\[(\d{1,3}):([0-5]\d)(?:\.(\d{1,3}))?\])");
  static const QRegularExpression offset(
      R"(\[offset:([+-]?\d{1,7})\])",
      QRegularExpression::CaseInsensitiveOption);
  const auto adjustment = offset.match(text);
  const qint64 shift =
      adjustment.hasMatch() ? adjustment.captured(1).toLongLong() : 0;
  QVariantList lines;
  for (const auto &raw : text.split('\n')) {
    auto matches = stamp.globalMatch(raw);
    QList<qint64> starts;
    int end = 0;
    while (matches.hasNext()) {
      auto m = matches.next();
      starts.append((m.captured(1).toLongLong() * 60 + m.captured(2).toInt()) *
                        1000 +
                    m.captured(3).leftJustified(3, '0').toInt() - shift);
      end = m.capturedEnd();
    }
    const auto words = raw.mid(end).trimmed();
    for (auto start : starts) {
      if (start < 0)
        start = 0;
      lines.append(QVariantMap{{"start", start}, {"end", 0}, {"text", words}});
      if (lines.size() > 4000)
        return {};
    }
  }
  std::stable_sort(lines.begin(), lines.end(),
                   [](const QVariant &a, const QVariant &b) {
                     return a.toMap().value("start").toLongLong() <
                            b.toMap().value("start").toLongLong();
                   });
  // Equal timestamps represent one line (e.g. a translation), never zero-length
  // rows.
  QVariantList merged;
  for (const auto &v : lines) {
    auto line = v.toMap();
    if (!merged.isEmpty() &&
        merged.last().toMap().value("start") == line.value("start")) {
      auto last = merged.last().toMap();
      last["text"] =
          last.value("text").toString() + "\n" + line.value("text").toString();
      merged.last() = last;
    } else
      merged.append(line);
  }
  bool words = false;
  for (int i = 0; i < merged.size(); ++i) {
    auto line = merged[i].toMap();
    line["end"] = i + 1 < merged.size()
                      ? merged[i + 1].toMap().value("start").toLongLong()
                      : qMax(duration, line.value("start").toLongLong() + 5000);
    words |= !line.value("text").toString().isEmpty();
    merged[i] = line;
  }
  return words ? merged : QVariantList{};
}
inline QString plain(const QVariantList &lines) {
  QStringList result;
  for (const auto &v : lines)
    result.append(v.toMap().value("text").toString());
  return result.join('\n');
}
} // namespace Lrc
