#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <functional>
#include <experimental/optional>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <curl/curl.h>
using namespace std;

experimental::optional<string> readFile(const string& path)
{
	ifstream stream(path);
	string returnable = "";
	if(stream.is_open())
	{
		string line;
		while(getline(stream, line)) returnable += line+"\n";
		return returnable;
	}
	return {};
}

vector<string> splitString(string input, const string& token)
{
	vector<string> returnable;
	const size_t tokLen = token.length();
	bool executed = false;
	function<void()> todo = [&executed, &input, &token, &tokLen, &todo, &returnable]()->void
	{
		for(size_t cur=0;cur<input.length()-tokLen;cur++)
		{
			executed = true;
			if(input.substr(cur, tokLen) == token)
			{
				returnable.push_back(input.substr(0, cur));
				input = input.substr(cur+tokLen);
				todo();
				break;
			}
		}
	};
	todo();
	if(executed) returnable.push_back(input);
	return returnable;
}

string getCurrentDateForEMail()
{
	time_t rawtime;
	time(&rawtime);
	tm time = *gmtime(&rawtime);
	char buf[30];
	strftime(&buf[0], sizeof(buf), "%a, %d %b %Y %H:%M:%S %Z", &time);
	return buf;
}

int main(int argc, char *argv[])
{
	if(argc == 6)
	{
		const string mailserver = argv[1];
		const string fromName = argv[2];
		const string fromEmail = argv[3];
		const vector<string> splitted = splitString(fromEmail, "@");
		if(splitted.size() != 2)
		{
			cout << "Invalid FROM e-mail address! Program will exit..." << endl;
			return 1;
		}
		const string mailserverDomain = splitted[1];
		const string to = argv[4];
		const string filename = argv[5];

		auto messageFileContents = readFile(filename);
		if(messageFileContents)
		{
			vector<string> headersAndBody = splitString(*messageFileContents, "\r\n\r\n\r\n");
			if(headersAndBody.size() != 2)
			{
				cout << "Cannot split message headers and message body! Program will exit..." << endl;
				return 1;
			}
			vector<string> parameters = splitString(headersAndBody[0], "\r\n");
			for(vector<string>::iterator it=parameters.begin();it!=parameters.end();it++)
			{
				vector<string> p = splitString(*it, ": ");
				if((p.size() == 2 && p[0] != "Subject" && p[0] != "Content-Type") || p.size() != 2)
				{
					parameters.erase(it);
					it--;
				}
			}
			stringstream uuidSS;
			uuidSS << boost::uuids::random_generator()();
			const string uuid = uuidSS.str();
			string newmsg;
			newmsg += "Date: "+getCurrentDateForEMail()+"\r\n";
			newmsg += "To: <"+to+">\r\n";
			newmsg += "From: \""+fromName+"\" <"+fromEmail+">\r\n";
			newmsg += "Message-ID: <"+uuid+"@"+mailserverDomain+">\r\n";
			for(vector<string>::iterator it=parameters.begin();it!=parameters.end();it++) newmsg += *it+"\r\n";
			newmsg += "\r\n\r\n";
			newmsg += headersAndBody[1];
			CURL *curl = curl_easy_init();
			curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_ALL);
			curl_easy_setopt(curl, CURLOPT_URL, mailserver.c_str());
			curl_easy_setopt(curl, CURLOPT_MAIL_FROM, fromEmail.c_str());
			struct curl_slist *recipients = NULL;
			recipients = curl_slist_append(recipients, to.c_str());
			curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);
			curl_easy_setopt(curl, CURLOPT_READFUNCTION, static_cast<size_t (*)(void*, size_t, size_t, void*)>([](void *ptr, size_t size, size_t nmemb, void *userp)->size_t
			{
				string* input = (string*)userp;
				if(size*nmemb<1) return 0;
				if(input->size())
				{
					*(char*)ptr = *input->c_str();
					input->erase(0, 1);
					return 1;
				}
				return 0;
			}));
			curl_easy_setopt(curl, CURLOPT_READDATA, &newmsg);
			curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
			//curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
			CURLcode res = curl_easy_perform(curl);
			curl_slist_free_all(recipients);
			curl_easy_cleanup(curl);
			if(res == CURLE_OK) cout << "Sent message successfully!" << endl;
			else cout << "Error sending message! " << curl_easy_strerror(res) << endl;
		}
		else cout << "Error reading file!" << endl;
	}
	else
	{
		cout << "Usage: "+string(argv[0])+" <mailserver> <fromName> <fromEmail> <to> <filename>" << endl;
	}
}