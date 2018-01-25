// db_labelsv1.cpp
//
// Read a current Penn database.

#include <iostream>
#include <string>
#include <utility>

#include "database.h"
#include "utils.h"
#include "io_primitives.h"
#include "bits.h"
#include "db_common.h"

using namespace std::literals::string_literals;

std::map<std::string, flag>
read_flags(istream &in)
{
  std::map<std::string, flag> flags;

  int count = db_read_this_labeled_int(in, "flagcount");

  for (int n = 0; n < count; n += 1) {
    flag f;
    auto name = db_read_this_labeled_string(in, "name");
    auto letter = db_read_this_labeled_string(in, "letter");
    f.name = name;
    f.letter = letter.size() ? letter.front() : '\0';
    f.types = split_words(db_read_this_labeled_string(in, "type"));
    f.perms = split_words(db_read_this_labeled_string(in, "perms"));
    f.negate_perms =
      split_words(db_read_this_labeled_string(in, "negate_perms"));
    flags.emplace(std::move(name), std::move(f));
  }

  count = db_read_this_labeled_int(in, "flagaliascount");
  for (int n = 0; n < count; n += 1) {
    auto name = db_read_this_labeled_string(in, "name");
    auto alias = db_read_this_labeled_string(in, "alias");

    auto orig = flags.find(name);
    if (orig != flags.end()) {
      flags.insert({alias, orig->second});
    }
  }

  return flags;
}

std::map<std::string, attrib>
read_db_attribs(istream &in)
{
  std::map<std::string, attrib> attribs;
  int count = db_read_this_labeled_int(in, "attrcount");

  for (int n = 0; n < count; n += 1) {
    attrib a;
    auto name = db_read_this_labeled_string(in, "name");
    a.name = name;
    a.flags = split_words(db_read_this_labeled_string(in, "flags"));
    a.creator = db_read_this_labeled_dbref(in, "creator");
    a.data = db_read_this_labeled_string(in, "data");
    attribs.emplace(std::move(name), std::move(a));
  }

  count = db_read_this_labeled_int(in, "attraliascount");
  for (int n = 0; n < count; n += 1) {
    auto name = db_read_this_labeled_string(in, "name");
    auto alias = db_read_this_labeled_string(in, "alias");

    auto orig = attribs.find(name);
    if (orig != attribs.end()) {
      attribs.insert({alias, orig->second});
    }
  }

  return attribs;
}

std::map<std::string, attrib>
read_obj_attribs(istream &in)
{
  std::map<std::string, attrib> attribs;
  int count = db_read_this_labeled_int(in, "attrcount");

  for (int n = 0; n < count; n += 1) {
    attrib a;
    auto name = db_read_this_labeled_string(in, "name");
    a.name = name;
    a.creator = db_read_this_labeled_dbref(in, "owner");
    a.flags = split_words(db_read_this_labeled_string(in, "flags"));
    a.derefs = db_read_this_labeled_int(in, "derefs");
    a.data = db_read_this_labeled_string(in, "value");
    attribs.emplace(std::move(name), std::move(a));
  }

  return attribs;
}

// Only works with DBF_SPIFFY_LOCKS
std::map<std::string, lock>
read_locks(istream &in, std::uint32_t flags)
{
  std::map<std::string, lock> locks;
  constexpr std::uint32_t fullspiff = DBF_LABELS | DBF_SPIFFY_LOCKS;
  
  int count = db_read_this_labeled_int(in, "lockcount");

  for (int n = 0; n < count; n += 1) {
    lock l;
    std::string type;
    if ((flags & fullspiff) == fullspiff ) {
      type = db_read_this_labeled_string(in, "type");
      l.type = type;     
      l.creator = db_read_this_labeled_dbref(in, "creator");
      l.flags = split_words(db_read_this_labeled_string(in, "flags"));
      l.derefs = db_read_this_labeled_int(in, "derefs");
      l.key = db_read_this_labeled_string(in, "key");
    } else if (flags & DBF_SPIFFY_LOCKS) {
      type = db_read_this_labeled_string(in, "type");
      l.type = type;
      l.creator = db_read_this_labeled_int(in, "creator");
      l.flags = lockbits_to_set(db_read_this_labeled_int(in, "flags"));
      l.key = db_read_this_labeled_string(in, "key");
    } else {
      throw db_format_exception{"Unsupported lock format."};
    }
    locks.emplace(std::move(type), std::move(l));
  }

  return locks;
}

dbthing
read_object(istream &in, dbref num, int version, std::uint32_t flags)
{
  dbthing obj{};
  obj.num = num;
  obj.name = db_read_this_labeled_string(in, "name");
  obj.location = db_read_this_labeled_dbref(in, "location");
  obj.contents = db_read_this_labeled_dbref(in, "contents");
  obj.exits = db_read_this_labeled_dbref(in, "exits");
  obj.next = db_read_this_labeled_dbref(in, "next");
  obj.parent = db_read_this_labeled_dbref(in, "parent");
  obj.locks = read_locks(in, flags);
  obj.owner = db_read_this_labeled_dbref(in, "owner");
  obj.zone = db_read_this_labeled_dbref(in, "zone");
  obj.pennies = db_read_this_labeled_int(in, "pennies");
  obj.type = dbtype_from_num(db_read_this_labeled_int(in, "type"));
  obj.flags = split_words(db_read_this_labeled_string(in, "flags"));
  obj.powers = split_words(db_read_this_labeled_string(in, "powers"));
  if (flags & DBF_WARNINGS) {
    obj.warnings = split_words(db_read_this_labeled_string(in, "warnings"));
  }
  if (flags & DBF_CREATION_TIMES) {
    obj.created = db_read_this_labeled_u32(in, "created");
    obj.modified = db_read_this_labeled_u32(in, "modified");
  }
  obj.attribs = read_obj_attribs(in);
  return obj;
}

std::tuple<database, int>
read_db_labelsv1(istream &in, std::uint32_t flags)
{
  std::uint32_t minimum_flags = DBF_LABELS | DBF_SPIFFY_LOCKS;

  if ((flags & minimum_flags) != minimum_flags) {
    // Pretty sure this should never happen.
    throw db_format_exception{"Invalid database format."};
  }

  database db;
  int dbversion = 1;

  if (flags & DBF_NEW_VERSIONS) {
    dbversion = db_read_this_labeled_int(in, "dbversion");
  }
  db.saved_time = db_read_this_labeled_string(in, "savedtime");

  char c;
  std::string line;
  while (in.get(c)) {
    switch (c) {
    case '+':
      std::getline(in, line);
      if (line == "FLAGS LIST") {
        db.flags = read_flags(in);
      } else if (line == "POWER LIST") {
        db.powers = read_flags(in);
      } else if (line == "ATTRIBUTES LIST") {
        db.attribs = read_db_attribs(in);
      } else {
        throw db_format_exception{"unknown +LIST: "s + line};
      }
      break;
    case '~': {
      long len = db_getref(in);
      db.objects.reserve(len);
    } break;
    case '!': {
      dbref d = db_getref(in);
      while (static_cast<std::size_t>(d) != db.objects.size()) {
        std::cerr << "Missing object #"
                  << db.objects.size()
                  << istream_line(in)
                  << '\n';
        dbthing garbage{};
        garbage.num = db.objects.size();
        db.objects.push_back(std::move(garbage));
      }
      db.objects.push_back(read_object(in, d, dbversion, flags));
    } break;
    case '*': {
      std::string eod;
      std::getline(in, eod);
      if (eod != "**END OF DUMP***") {
        throw db_format_exception{"Invalid end string: *"s + eod};
      }
    } break;
    default:
      throw db_format_exception{"Unexpected character: "s + c};
    }
  }
  return {db, dbversion};
}

void
write_flags(std::ostream &out, const std::map<std::string, flag> &flags)
{
  std::vector<std::string> canon;
  std::vector<std::string> aliases;

  for (const auto &flag : flags) {
    if (flag.first == flag.second.name) {
      canon.push_back(flag.first);
    } else {
      aliases.push_back(flag.first);
    }
  }

  out << "flagcount " << canon.size() << '\n';
  for (const auto &name : canon) {
    const auto &flag = flags.find(name)->second;
    out << " name \"" << flag.name << "\"\n";
    if (flag.letter) {
      out << "  letter \"" << flag.letter << "\"\n";
    } else {
      out << "  letter \"\"\n";
    }
    out << "  type \"" << join_words(flag.types) << "\"\n";
    out << "  perms \"" << join_words(flag.perms) << "\"\n";
    out << "  negate_perms \"" << join_words(flag.negate_perms) << "\"\n";
  }

  out << "flagaliascount " << aliases.size() << '\n';
  for (const auto &name : aliases) {
    const auto &flag = flags.find(name)->second;
    out << " name \"" << flag.name << "\"\n";
    out << "  alias \"" << name << "\"\n";
  }
}

void
write_db_attribs(std::ostream &out,
                 const std::map<std::string, attrib> &attribs)
{
  std::vector<std::string> canon;
  std::vector<std::string> aliases;

  for (const auto &attr : attribs) {
    if (attr.first == attr.second.name) {
      canon.push_back(attr.first);
    } else {
      aliases.push_back(attr.first);
    }
  }

  out << "attrcount " << canon.size() << '\n';
  for (const auto &name : canon) {
    const auto &attr = attribs.find(name)->second;
    out << " name \"" << attr.name << "\"\n";
    out << "  flags \"" << join_words(attr.flags) << "\"\n";
    out << "  creator #" << attr.creator << '\n';
    out << "  data \"\"\n";
  }

  out << "attraliascount " << aliases.size() << '\n';
  for (const auto &name : aliases) {
    const auto &attr = attribs.find(name)->second;
    out << " name \"" << attr.name << "\"\n";
    out << "  alias \"" << name << "\"\n";
  }
}

void
write_locks(std::ostream &out, const std::map<std::string, lock> &locks)
{
  out << "lockcount " << locks.size() << '\n';
  for (const auto &l : locks) {
    const auto &lk = l.second;
    out << " type \"" << lk.type << "\"\n";
    out << "  creator #" << lk.creator << '\n';
    out << "  flags \"" << join_words(lk.flags) << "\"\n";
    out << "  derefs " << lk.derefs << '\n';
    db_write_labeled_string(out, "  key", lk.key);
  }
}

void
write_obj_attribs(std::ostream &out,
                  const std::map<std::string, attrib> &attribs)
{
  out << "attrcount " << attribs.size() << '\n';
  for (const auto &a : attribs) {
    const auto &attr = a.second;
    out << " name \"" << attr.name << "\"\n";
    out << "  owner #" << attr.creator << '\n';
    out << "  flags \"" << join_words(attr.flags) << "\"\n";
    out << "  derefs " << attr.derefs << '\n';
    db_write_labeled_string(out, "  value", attr.data);
  }
}

void
write_db_labelsv1(std::ostream &out, const database &db)
{
  int dbflag = 5;
  dbflag += DBF_NO_CHAT_SYSTEM;
  dbflag += DBF_WARNINGS;
  dbflag += DBF_CREATION_TIMES;
  dbflag += DBF_SPIFFY_LOCKS;
  dbflag += DBF_NEW_STRINGS;
  dbflag += DBF_TYPE_GARBAGE;
  dbflag += DBF_SPLIT_IMMORTAL;
  dbflag += DBF_NO_TEMPLE;
  dbflag += DBF_LESS_GARBAGE;
  dbflag += DBF_AF_VISUAL;
  dbflag += DBF_VALUE_IS_COST;
  dbflag += DBF_LINK_ANYWHERE;
  dbflag += DBF_NO_STARTUP_FLAG;
  dbflag += DBF_AF_NODUMP;
  dbflag += DBF_NEW_FLAGS;
  dbflag += DBF_NEW_POWERS;
  dbflag += DBF_POWERS_LOGGED;
  dbflag += DBF_LABELS;
  dbflag += DBF_SPIFFY_AF_ANSI;
  dbflag += DBF_HEAR_CONNECT;
  dbflag += DBF_NEW_VERSIONS;

  dbflag = dbflag * 256 + 2;

  out << "+V" << dbflag << '\n';
  out << "dbversion 5\n";
  
  out << "savedtime \"" << get_time() << "\"\n";

  out << "+FLAGS LIST\n";
  write_flags(out, db.flags);
  out << "+POWER LIST\n";
  write_flags(out, db.powers);
  out << "+ATTRIBUTES LIST\n";
  write_db_attribs(out, db.attribs);
  out << '~' << db.objects.size() << '\n';
  for (const auto &obj : db.objects) {
    out << '!' << obj.num << '\n';
    db_write_labeled_string(out, "name", obj.name);
    out << "location #" << obj.location << '\n';
    out << "contents #" << obj.contents << '\n';
    out << "exits #" << obj.exits << '\n';
    out << "next #" << obj.next << '\n';
    out << "parent #" << obj.parent << '\n';
    write_locks(out, obj.locks);
    out << "owner #" << obj.owner << '\n';
    out << "zone #" << obj.zone << '\n';
    out << "pennies " << obj.pennies << '\n';
    out << "type " << dbtype_to_num(obj.type) << '\n';
    out << "flags \"" << join_words(obj.flags) << "\"\n";
    out << "powers \"" << join_words(obj.powers) << "\"\n";
    out << "warnings \"" << join_words(obj.warnings) << "\"\n";
    out << "created " << obj.created << '\n';
    out << "modified " << obj.modified << '\n';
    write_obj_attribs(out, obj.attribs);
  }
  out << "***END OF DUMP***\n";
}
