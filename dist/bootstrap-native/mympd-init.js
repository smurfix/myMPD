import Alert from '../components/alert-native.js';
import Button from '../components/button-native.js';
import Carousel from '../components/carousel-native.js';
import Collapse from '../components/collapse-native.js';
import Dropdown from '../components/dropdown-native.js';
import Modal from '../components/modal-native.js';
import Offcanvas from '../components/offcanvas-native.js';
import Popover from '../components/popover-native.js';
import Tab from '../components/tab-native.js';
import Toast from '../components/toast-native.js';

const componentsInit = {
  Alert: Alert.init,
  Button: Button.init,
  Carousel: Carousel.init,
  Collapse: Collapse.init,
  Dropdown: Dropdown.init,
  Modal: Modal.init,
  Offcanvas: Offcanvas.init,
  Popover: Popover.init,
  Tab: Tab.init,
  Toast: Toast.init,
};

function initializeDataAPI(Konstructor, collection) {
  Array.from(collection).forEach((x) => new Konstructor(x));
}
function removeElementDataAPI(component, collection) {
  Array.from(collection).forEach((x) => {
    if (x[component]) x[component].dispose();
  });
}

export function initCallback(context) {
  const lookUp = context instanceof Element ? context : document;

  Object.keys(componentsInit).forEach((comp) => {
    const { constructor, selector } = componentsInit[comp];
    initializeDataAPI(constructor, lookUp.querySelectorAll(selector));
  });
}

export function removeDataAPI(context) {
  const lookUp = context instanceof Element ? context : document;

  Object.keys(componentsInit).forEach((comp) => {
    const { component, selector } = componentsInit[comp];
    removeElementDataAPI(component, lookUp.querySelectorAll(selector));
  });
}

// bulk initialize all components
if (document.body) initCallback();
else {
  document.addEventListener('DOMContentLoaded', () => initCallback(), { once: true });
}
