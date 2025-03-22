import { onContentUpdate } from "./framework/router.js";
import { toggleClasses } from "./framework/style-managment.js";

// let currentURI = "/launch";
// const titles = {
//     "/launch" : "Launch",
//     "/flight-data" : "Flight Data",
//     "/debug" : "Debug du pas de tir",
// };


onContentUpdate((uri) => {
    toggleClasses(
        'nav a', 
        (navLink) => navLink.getAttribute("href") === uri,
        'bg-gray-200 hover:bg-gray-300 text-gray-900 font-semibold', 
        'hover:bg-gray-200 text-gray-700'
    );
});
