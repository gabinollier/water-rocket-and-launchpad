# Global instructions

## When coding in C++

- You should write brackets on their on line.
- You should not add any comments if that's not really useful or asked by the user.-
- You should follow the coding style of the rest of the file : function naming, function order, etc.

## When coding in HTML/JS

- You should alaways use TailwindCSS V4 for styling. It is already installed and running.
- You should not add any comments if that's not really useful or asked by the user.
- We use a home-made framework where pages other than `index.html` are automatically injected into the `index.html`.The `.html` file is fetched and injected into the `content` div, and the corresponding `.js` file is imported at the first load of the page. Each one these `.js` files need to implement the `export const onPageLoad = () => {}` function that is called by the framework at each load of the page (not only the first one). These `html` pages are found in the `frontend/src/pages/` folder and their corresponding `js` files are found in the `frontend/src/js/pages/` folder. You don't need to import any `css` in these `html` files, it's already done by the `index.html`. You can import `js` modules like `frontend/src/js/modules/websocket.js` if needed, or create new ones.
