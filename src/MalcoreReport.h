#pragma once

#include <QString>
#include <QJsonObject>
#include <QJsonArray>

/*
Requirements:
- Threat score + signatures
- IOCs
- Hashes
- Yara Rule (TODO: list other rules that matched?)
- Explain threat score
- Packer Information
- Dynamic analysis
    - Return address
        - Option to navigate there in x64dbg
    - Function + Arguments
*/
struct MalcoreAnalysis
{
    MalcoreAnalysis(QJsonObject root)
        : mData(std::move(root))
    {
        open("head");
        open("style");
        mReport += R"(
ul {
  margin-left: -25px;
}
td {
  padding-right: 5px;
}
)";
        close("style");
        close("head");
    }

    QString getReportHtml()
    {
        threatSummary();
        dynamicAnalysis();
        IOCs();
        yaraRule();
        packerInformation();
        return mReport;
    }

private:
    void open(const char* tag)
    {
        mReport += QString("<%1>").arg(tag);
    }

    void close(const char* tag)
    {
        mReport += QString("</%1>").arg(tag);
    }

    void tag(const char* tag, const QString& content)
    {
        QStringList tags = QString(tag).split('/');
        for(int i = 0; i < tags.length(); i++)
        {
            open(tags[i].toUtf8().constData());
        }

        mReport += content.toHtmlEscaped();

        for(int i = 0; i < tags.length(); i++)
        {
            close(tags[tags.length() - i - 1].toUtf8().constData());
        }
    }

    void p(const QString& s)
    {
        tag("p", s);
    }

    void p(const QString& key, const QString& value)
    {
        p(QString("%1: %2").arg(key, value));
    }

    void section(const QString& title)
    {
        tag("h1", title);
    }

    void yaraRule()
    {
        QJsonArray yara = mData["yara_rules"].toObject()["results"].toArray();
        for(int i = 0; i < yara.size(); i++)
        {
            auto entry = yara[i].toArray();
            auto key = entry[0].toString();
            auto value = entry[1].toString();
            if(value.startsWith("rule "))
            {
                section("Yara rule");
                tag("pre", value.replace("\t", "  "));
                return;
            }
        }
    }

    void threatSummary()
    {
        QJsonObject data = mData["threat_summary"].toObject()["results"].toObject();

        section("Threat Summary");
        {
            QJsonObject threat = data["threat_level"].toObject();
            auto score = threat["score"].toString();
            p("Score", score);
            p("Indicators:");
            open("ul");
            auto signatures = threat["signatures"].toArray();
            for(int i = 0; i < signatures.size(); i++)
            {
                tag("li/span", signatures[i].toString());
            }
            close("ul");
        }
    }

    void IOCs()
    {
        section("IOCs");
        {
            tag("h2", "Hashes");
            open("p");
            QJsonObject hashes = mData["hashes"].toObject();
            open("table");

            open("tr");
            tag("td/u", "Algorithm");
            tag("td/u", "Hash");
            close("tr");

            for(const QString& key : hashes.keys())
            {
                auto value = hashes[key].toString();

                open("tr");
                tag("td/b", key);
                tag("td/code", value);
                close("tr");
            }
            close("table");
            close("p");

            tag("h2", "Strings");
            QJsonObject data = mData["threat_summary"].toObject()["results"].toObject();
            QJsonObject iocs = data["iocs"].toObject();
            auto strings = iocs["strings"].toArray();
            open("ul");
            for(int i = 0; i < strings.size(); i++)
            {
                auto entry = strings[i].toString();
                tag("li/code", entry);
            }
            close("ul");
        }
    }

    void packerInformation()
    {
        section("Packer Information");
        auto info = mData["packer_information"].toObject()["results"].toArray();
        open("ul");
        for(int i = 0; i < info.size(); i++)
        {
            auto entry = info[i].toObject();
            auto percent = entry["percent"].toString();
            auto name = entry["packer_name"].toString();
            tag("li/span", QString("[%1] %2").arg(percent, name));
        }
        close("ul");
    }

    void dynamicAnalysis()
    {
        section("Dynamic Analysis");
        open("p");
        open("table");

        // Header
        open("tr");
        tag("td/u", "Return Address");
        tag("td/u", "Module");
        tag("td/u", "Function");
        close("tr");

        QJsonArray analysis = mData["dynamic_analysis"].toObject()["parsed_output"].toArray();
        for(int i = 0; i < analysis.count(); i++)
        {
            open("tr");

            QJsonObject entry = analysis[i].toObject();
            auto location = entry["location"].toString();

            auto makeAddressLink = [](const QString& str)
            {
                // TODO: find all 0x prefixes and do this conversion
                bool ok = false;
                auto value = str.toULongLong(&ok, 0);
                if(!ok)
                {
                    return str.toHtmlEscaped();
                }
                else
                {
                    char hex[64]="";
                    sprintf_s(hex, "0x%llX", value);
                    QString result = "<a href=\"address://";
                    result += hex;
                    result += "\">";
                    result += str.toHtmlEscaped();
                    result += "</a>";
                    return result;
                }
            };

            open("td");
            open("code");
            mReport += makeAddressLink(location);
            close("code");
            close("td");

            auto dll = entry["dll_name"].toString();
            tag("td/p", dll);

            open("td");
            open("code");
            QString summary = "";

            auto function = entry["function_called"].toString();
            auto suspicious = entry["known_suspicious_function"].toBool();
            if(suspicious)
            {
                summary += "<span style=\"color: orange;\">";
            }
            summary += "<b>";
            summary += function.toHtmlEscaped();
            summary += "</b>";
            if(suspicious)
            {
                summary += "</span>";
            }
            summary += "(";

            auto arguments = entry["arguments_passed"].toArray();
            for(int j = 0; j < arguments.count(); j++)
            {
                if(j > 0)
                {
                    summary += ", ";
                }
                auto arg = arguments[j].toString();
                //summary += makeAddressLink(arg);
                summary += arg.toHtmlEscaped();
            }
            summary += ")";

            auto result = entry["function_return_value"].toString();
            if(!result.isEmpty() && result.toLower() != "none")
            {
                summary += " -> ";
                summary += makeAddressLink(result);
            }

            mReport += summary;

            close("code");
            close("td");

            close("tr");
        }

        close("table");
        close("p");
    }

private:
    QString mReport;
    QJsonObject mData;
};
