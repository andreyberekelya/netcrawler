#include <libxml/tree.h>
#include <libxml/HTMLparser.h>
#include <libxml++/libxml++.h>

#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>

#include <glibmm.h>

#include <string>
#include <functional>
#include <list>

#include <thread>
#include <mutex>
#include <condition_variable>
#include <utility>
#include <fstream>

#define HEADER_ACCEPT "Accept:text/html"
#define HEADER_USER_AGENT "User-Agent:Chrome/99.0.4312.70"

std::mutex threadMutex;
std::condition_variable conditionVariable;
std::list<std::string> threadFinished;
std::list<std::string> parsedData;

std::string getUrl( const std::string &url ) {
	curlpp::Easy request;
	request.setOpt(curlpp::options::Url(url));

	std::list<std::string> headers;
	headers.emplace_back(HEADER_ACCEPT);
	headers.emplace_back(HEADER_USER_AGENT);
	request.setOpt(new curlpp::options::HttpHeader(headers));
	request.setOpt(new curlpp::options::FollowLocation(true));

	std::ostringstream responseStream;
	curlpp::options::WriteStream streamWriter(&responseStream);
	request.setOpt(streamWriter);

	request.perform();
	return responseStream.str();
}

void getContent( const std::string &url, const std::string &xpath, std::function<void( xmlpp::Node * )> parser ) {
	std::string re = getUrl(url);

	xmlDoc *doc = htmlReadDoc((xmlChar *) re.c_str(), nullptr, "UTF-8",
	                          HTML_PARSE_RECOVER | HTML_PARSE_NOERROR | HTML_PARSE_NOWARNING);

	xmlNode *r = xmlDocGetRootElement(doc);
	auto *root = new xmlpp::Element(r);

	try {
		auto elements = root->find(xpath);
		std::for_each(elements.begin(), elements.end(), std::move(parser));
	} catch( Glib::ConvertError &e ) {
		std::cerr << e.what() << std::endl;
	}

	delete root;
	xmlFreeDoc(doc);
}

void worker( const std::string &rubric, const std::string &url, const std::string &suburl ) {
	std::string result;
	getContent(url + suburl, "//*[@class=\"card-mini__text\"]", [&]( xmlpp::Node *node ) {

		const auto *element = dynamic_cast<const xmlpp::Element *>(node->get_first_child());
		std::string name(element->get_child_text()->get_content());
		std::string time(
			dynamic_cast<const xmlpp::Element *>(element->get_next_sibling()->get_first_child())->get_child_text()->get_content());

		result.append("\"").append(rubric).append("\";\"").append(name).append("\";").append(time).append("\n");
	});

	std::unique_lock<std::mutex> lock(threadMutex);
	parsedData.emplace_back(result);
	threadFinished.emplace_back(suburl);
	conditionVariable.notify_one();
}

int main() {
	std::setlocale(LC_ALL, "ru_RU.UTF-8");
	std::setlocale(LC_NUMERIC, "ru_RU.UTF-8");
	std::setlocale(LC_TIME, "ru_RU.UTF-8");

	std::string url = "https://www.lenta.ru";
	std::map<std::string, std::thread *> threadMap;

	std::unique_lock<std::mutex> lock(threadMutex);
	getContent(url, "//*[@class=\"menu__nav-list\"]/li/a", [&]( xmlpp::Node *node ) {
		const auto *element = dynamic_cast<const xmlpp::Element *>(node);
		std::string name(element->get_child_text()->get_content());
		std::string suburl(element->get_attribute("href")->get_value());
		if( suburl == "/" ) return;

		if( suburl.substr(0, 4) != "http" ) {
			threadMap[suburl] = new std::thread(worker, name, url, suburl);
		}
	});

	std::ofstream dataFile;
	dataFile.open("news.csv");
	while( !threadMap.empty()) {
		conditionVariable.wait(lock);
		for( const auto &th: threadFinished ) {
			auto it = threadMap.find(th);
			if( it != threadMap.end()) {
				if( it->second->joinable()) {
					it->second->join();
					delete it->second;
				}
				threadMap.erase(it);
			}
		}
		for( const auto &data: parsedData ) {
			dataFile << data;
		}
		threadFinished.clear();
		parsedData.clear();
	}
	dataFile.close();

	return 0;
}
