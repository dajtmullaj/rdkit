//
//  Copyright (C) 2015-2019 Greg Landrum
//
//   @@ All Rights Reserved @@
//  This file is part of the RDKit.
//  The contents are covered by the terms of the BSD license
//  which is included in the file license.txt, found at the root
//  of the RDKit source tree.
//
// derived from Dave Cosgrove's MolDraw2D
//

#include <GraphMol/MolDraw2D/MolDraw2DSVG.h>
#include <GraphMol/MolDraw2D/DrawText.h>
#include <GraphMol/SmilesParse/SmilesWrite.h>
#include <Geometry/point.h>
#ifdef RDK_BUILD_FREETYPE_SUPPORT
#include <GraphMol/MolDraw2D/DrawTextFTSVG.h>
#else
#include <GraphMol/MolDraw2D/DrawTextSVG.h>
#endif

#include <boost/format.hpp>
#include <sstream>

namespace RDKit {
std::string DrawColourToSVG(const DrawColour &col) {
  const char *convert = "0123456789ABCDEF";
  std::string res(7, ' ');
  res[0] = '#';
  unsigned int v;
  unsigned int i = 1;
  v = int(255 * col.r);
  if (v > 255) {
    throw ValueErrorException(
        "elements of the color should be between 0 and 1");
  }
  res[i++] = convert[v / 16];
  res[i++] = convert[v % 16];
  v = int(255 * col.g);
  if (v > 255) {
    throw ValueErrorException(
        "elements of the color should be between 0 and 1");
  }
  res[i++] = convert[v / 16];
  res[i++] = convert[v % 16];
  v = int(255 * col.b);
  if (v > 255) {
    throw ValueErrorException(
        "elements of the color should be between 0 and 1");
  }
  res[i++] = convert[v / 16];
  res[i++] = convert[v % 16];
  return res;
}

// ****************************************************************************
void MolDraw2DSVG::initDrawing() {
  d_os << "<?xml version='1.0' encoding='iso-8859-1'?>\n";
  d_os << "<svg version='1.1' baseProfile='full'\n      \
        xmlns='http://www.w3.org/2000/svg'\n              \
        xmlns:rdkit='http://www.rdkit.org/xml'\n              \
        xmlns:xlink='http://www.w3.org/1999/xlink'\n          \
        xml:space='preserve'\n";
  d_os << boost::format{"width='%1%px' height='%2%px' viewBox='0 0 %1% %2%'>\n"}
      % width() % height();
  d_os << "<!-- END OF HEADER -->\n";

  // d_os<<"<g transform='translate("<<width()*.05<<","<<height()*.05<<")
  // scale(.85,.85)'>";
}

// ****************************************************************************
void MolDraw2DSVG::initTextDrawer(bool noFreetype) {
  double max_fnt_sz = drawOptions().maxFontSize;
  double min_fnt_sz = drawOptions().minFontSize;

  if (noFreetype) {
    text_drawer_.reset(
        new DrawTextSVG(max_fnt_sz, min_fnt_sz, d_os, d_activeClass));
  } else {
#ifdef RDK_BUILD_FREETYPE_SUPPORT
    try {
      text_drawer_.reset(new DrawTextFTSVG(
          max_fnt_sz, min_fnt_sz, drawOptions().fontFile, d_os, d_activeClass));
    } catch (std::runtime_error &e) {
      BOOST_LOG(rdWarningLog)
          << e.what() << std::endl
          << "Falling back to native SVG text handling." << std::endl;
      text_drawer_.reset(
          new DrawTextSVG(max_fnt_sz, min_fnt_sz, d_os, d_activeClass));
    }
#else
    text_drawer_.reset(
        new DrawTextSVG(max_fnt_sz, min_fnt_sz, d_os, d_activeClass));
#endif
  }
}

// ****************************************************************************
void MolDraw2DSVG::finishDrawing() {
  // d_os << "</g>";
  d_os << "</svg>\n";
}

// ****************************************************************************
void MolDraw2DSVG::setColour(const DrawColour &col) {
  MolDraw2D::setColour(col);
}

void MolDraw2DSVG::drawWavyLine(const Point2D &cds1, const Point2D &cds2,
                                const DrawColour &col1, const DrawColour &col2,
                                unsigned int nSegments, double vertOffset) {
  PRECONDITION(nSegments > 1, "too few segments");
  RDUNUSED_PARAM(col2);

  if (nSegments % 2) {
    ++nSegments;  // we're going to assume an even number of segments
  }
  setColour(col1);

  Point2D delta = (cds2 - cds1);
  Point2D perp(delta.y, -delta.x);
  perp.normalize();
  perp *= vertOffset;
  delta /= nSegments;

  Point2D c1 = getDrawCoords(cds1);

  std::string col = DrawColourToSVG(colour());
  double width = getDrawLineWidth();
  d_os << "<path ";
  if (d_activeClass != "") {
    d_os << "class='" << d_activeClass << "' ";
  }
  d_os << "d='M" << c1.x << "," << c1.y;
  for (unsigned int i = 0; i < nSegments; ++i) {
    Point2D startpt = cds1 + delta * i;
    Point2D segpt = getDrawCoords(startpt + delta);
    Point2D cpt1 =
        getDrawCoords(startpt + delta / 3. + perp * (i % 2 ? -1 : 1));
    Point2D cpt2 =
        getDrawCoords(startpt + delta * 2. / 3. + perp * (i % 2 ? -1 : 1));
    d_os << " C" << cpt1.x << "," << cpt1.y << " " << cpt2.x << "," << cpt2.y
         << " " << segpt.x << "," << segpt.y;
  }
  d_os << "' ";

  d_os << "style='fill:none;stroke:" << col
       << ";stroke-width:" << boost::format("%.1f") % width
       << "px;stroke-linecap:butt;stroke-linejoin:miter;stroke-opacity:1"
       << "'";
  d_os << " />\n";
}

// ****************************************************************************
void MolDraw2DSVG::drawBond(
    const ROMol &mol, const Bond *bond, int at1_idx, int at2_idx,
    const std::vector<int> *highlight_atoms,
    const std::map<int, DrawColour> *highlight_atom_map,
    const std::vector<int> *highlight_bonds,
    const std::map<int, DrawColour> *highlight_bond_map,
    const std::vector<std::pair<DrawColour, DrawColour>> *bond_colours) {
  PRECONDITION(bond, "bad bond");
  std::string o_class = d_activeClass;
  if (!d_activeClass.empty()) {
    d_activeClass += " ";
  }
  d_activeClass += boost::str(boost::format("bond-%d") % bond->getIdx());
  MolDraw2D::drawBond(mol, bond, at1_idx, at2_idx, highlight_atoms,
                      highlight_atom_map, highlight_bonds, highlight_bond_map,
                      bond_colours);
  d_activeClass = o_class;
};

// ****************************************************************************
void MolDraw2DSVG::drawAtomLabel(int atom_num, const DrawColour &draw_colour) {
  std::string o_class = d_activeClass;
  if (!d_activeClass.empty()) {
    d_activeClass += " ";
  }
  d_activeClass += boost::str(boost::format("atom-%d") % atom_num);
  MolDraw2D::drawAtomLabel(atom_num, draw_colour);
  d_activeClass = o_class;
}

// ****************************************************************************
void MolDraw2DSVG::drawAnnotation(const AnnotationType &annot) {
  std::string o_class = d_activeClass;
  if (!d_activeClass.empty()) {
    d_activeClass += " ";
  }
  d_activeClass += "note";
  MolDraw2D::drawAnnotation(annot);
  d_activeClass = o_class;
}

// ****************************************************************************
void MolDraw2DSVG::drawLine(const Point2D &cds1, const Point2D &cds2) {
  Point2D c1 = getDrawCoords(cds1);
  Point2D c2 = getDrawCoords(cds2);
  std::string col = DrawColourToSVG(colour());
  double width = getDrawLineWidth();
  std::string dashString = "";
  const DashPattern &dashes = dash();
  if (dashes.size()) {
    std::stringstream dss;
    dss << ";stroke-dasharray:";
    std::copy(dashes.begin(), dashes.end() - 1,
              std::ostream_iterator<double>(dss, ","));
    dss << dashes.back();
    dashString = dss.str();
  }
  d_os << "<path ";
  if (!d_activeClass.empty()) {
    d_os << "class='" << d_activeClass << "' ";
  }
  d_os << "d='M " << c1.x << "," << c1.y << " L " << c2.x << "," << c2.y
       << "' ";
  d_os << "style='fill:none;fill-rule:evenodd;stroke:" << col
       << ";stroke-width:" << boost::format("%.1f") % width
       << "px;stroke-linecap:butt;stroke-linejoin:miter;stroke-opacity:1"
       << dashString << "'";
  d_os << " />\n";
}

// ****************************************************************************
void MolDraw2DSVG::drawPolygon(const std::vector<Point2D> &cds) {
  PRECONDITION(cds.size() >= 3, "must have at least three points");

  std::string col = DrawColourToSVG(colour());
  unsigned int width = getDrawLineWidth();
  std::string dashString = "";
  d_os << "<path ";
  if (d_activeClass != "") {
    d_os << "class='" << d_activeClass << "' ";
  }
  d_os << "d='M";
  Point2D c0 = getDrawCoords(cds[0]);
  d_os << " " << c0.x << "," << c0.y;
  for (unsigned int i = 1; i < cds.size(); ++i) {
    Point2D ci = getDrawCoords(cds[i]);
    d_os << " L " << ci.x << "," << ci.y;
  }
  if (fillPolys()) {
    // the Z closes the path which we don't want for unfilled polygons
    d_os << " Z' style='fill:" << col
         << ";fill-rule:evenodd;fill-opacity:" << colour().a << ";";
  } else {
    d_os << "' style='fill:none;";
  }

  d_os << "stroke:" << col << ";stroke-width:" << boost::format("%.1f") % width
       << "px;stroke-linecap:butt;stroke-linejoin:miter;stroke-opacity:"
       << colour().a << ";" << dashString << "'";
  d_os << " />\n";
}

// ****************************************************************************
void MolDraw2DSVG::drawEllipse(const Point2D &cds1, const Point2D &cds2) {
  Point2D c1 = getDrawCoords(cds1);
  Point2D c2 = getDrawCoords(cds2);
  double w = c2.x - c1.x;
  double h = c2.y - c1.y;
  double cx = c1.x + w / 2;
  double cy = c1.y + h / 2;
  w = w > 0 ? w : -1 * w;
  h = h > 0 ? h : -1 * h;

  std::string col = DrawColourToSVG(colour());
  unsigned int width = getDrawLineWidth();
  std::string dashString = "";
  d_os << "<ellipse"
       << " cx='" << cx << "'"
       << " cy='" << cy << "'"
       << " rx='" << w / 2 << "'"
       << " ry='" << h / 2 << "'";

  if (d_activeClass != "") {
    d_os << " class='" << d_activeClass << "'";
  }
  d_os << " style='";
  if (fillPolys()) {
    d_os << "fill:" << col << ";fill-rule:evenodd;";
  } else {
    d_os << "fill:none;";
  }

  d_os << "stroke:" << col << ";stroke-width:" << boost::format("%.1f") % width
       << "px;stroke-linecap:butt;stroke-linejoin:miter;stroke-opacity:1"
       << dashString << "'";
  d_os << " />\n";
}

// ****************************************************************************
void MolDraw2DSVG::clearDrawing() {
  std::string col = DrawColourToSVG(drawOptions().backgroundColour);
  d_os << "<rect";
  d_os << " style='opacity:1.0;fill:" << col << ";stroke:none'";
  d_os << " width='" << width() << "' height='" << height() << "'";
  d_os << " x='" << offset().x << "' y='" << offset().y << "'";
  d_os << "> </rect>\n";
}

// ****************************************************************************
static const char *RDKIT_SVG_VERSION = "0.9";
void MolDraw2DSVG::addMoleculeMetadata(const ROMol &mol, int confId) const {
  PRECONDITION(d_os, "no output stream");
  d_os << "<metadata>" << std::endl;
  d_os << "<rdkit:mol"
       << " xmlns:rdkit = \"http://www.rdkit.org/xml\""
       << " version=\"" << RDKIT_SVG_VERSION << "\""
       << ">" << std::endl;
  for (const auto atom : mol.atoms()) {
    d_os << "<rdkit:atom idx=\"" << atom->getIdx() + 1 << "\"";
    bool doKekule = false, allHsExplicit = true, isomericSmiles = true;
    d_os << " atom-smiles=\""
         << SmilesWrite::GetAtomSmiles(atom, doKekule, nullptr, allHsExplicit,
                                       isomericSmiles)
         << "\"";
    auto tag = boost::str(boost::format("_atomdrawpos_%d") % confId);

    const Conformer &conf = mol.getConformer(confId);
    RDGeom::Point3D pos = conf.getAtomPos(atom->getIdx());

    Point2D dpos(pos.x, pos.y);
    if (atom->hasProp(tag)) {
      dpos = atom->getProp<Point2D>(tag);
    } else {
      dpos = getDrawCoords(dpos);
    }
    d_os << " drawing-x=\"" << dpos.x << "\""
         << " drawing-y=\"" << dpos.y << "\"";
    d_os << " x=\"" << pos.x << "\""
         << " y=\"" << pos.y << "\""
         << " z=\"" << pos.z << "\"";

    d_os << " />" << std::endl;
  }
  for (const auto bond : mol.bonds()) {
    d_os << "<rdkit:bond idx=\"" << bond->getIdx() + 1 << "\"";
    d_os << " begin-atom-idx=\"" << bond->getBeginAtomIdx() + 1 << "\"";
    d_os << " end-atom-idx=\"" << bond->getEndAtomIdx() + 1 << "\"";
    bool doKekule = false, allBondsExplicit = true;
    d_os << " bond-smiles=\""
         << SmilesWrite::GetBondSmiles(bond, -1, doKekule, allBondsExplicit)
         << "\"";
    d_os << " />" << std::endl;
  }
  d_os << "</rdkit:mol></metadata>" << std::endl;
}

void MolDraw2DSVG::addMoleculeMetadata(const std::vector<ROMol *> &mols,
                                       const std::vector<int> confIds) const {
  for (unsigned int i = 0; i < mols.size(); ++i) {
    int confId = -1;
    if (confIds.size() == mols.size()) {
      confId = confIds[i];
    }
    addMoleculeMetadata(*(mols[i]), confId);
  }
};

void MolDraw2DSVG::tagAtoms(const ROMol &mol, double radius,
                            const std::map<std::string, std::string> &events) {
  PRECONDITION(d_os, "no output stream");
  // first bonds so that they are under the atoms
  for (const auto &bond : mol.bonds()) {
    auto this_idx = bond->getIdx();
    auto a1pos = getDrawCoords(atomCoords()[bond->getBeginAtomIdx()]);
    auto a2pos = getDrawCoords(atomCoords()[bond->getEndAtomIdx()]);
    auto width = 2 + lineWidth();
    d_os << "<path "
         << " d='M " << a1pos.x << "," << a1pos.y << " L " << a2pos.x << ","
         << a2pos.y << "'";
    d_os << " class='bond-selector bond-" << this_idx;
    if (d_activeClass != "") {
      d_os << " " << d_activeClass;
    }
    d_os << "'";
    d_os << " style='fill:#fff;stroke:#fff;stroke-width:"
         << boost::format("%.1f") % width
         << "px;fill-opacity:0;"
            "stroke-opacity:0' ";
    d_os << "/>\n";
  }
  for (const auto &at : mol.atoms()) {
    auto this_idx = at->getIdx();
    auto pos = getDrawCoords(atomCoords()[this_idx]);
    d_os << "<circle "
         << " cx='" << pos.x << "'"
         << " cy='" << pos.y << "'"
         << " r='" << (scale() * radius) << "'";
    d_os << " class='atom-selector atom-" << this_idx;
    if (d_activeClass != "") {
      d_os << " " << d_activeClass;
    }
    d_os << "'";
    d_os << " style='fill:#fff;stroke:#fff;stroke-width:1px;fill-opacity:0;"
            "stroke-opacity:0' ";
    for (const auto &event : events) {
      d_os << " " << event.first << "='" << event.second << "(" << this_idx
           << ");"
           << "'";
    }
    d_os << "/>\n";
  }
}
}  // namespace RDKit
